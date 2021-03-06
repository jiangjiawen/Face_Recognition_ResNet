// Face_Recognition_ResNet.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <dlib/dnn.h>
#include <dlib/gui_widgets.h>
#include <dlib/clustering.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>

using namespace dlib;
using namespace std;

template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = add_prev1<block<N, BN, 1, tag1<SUBNET>>>;

template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = add_prev2<avg_pool<2, 2, 2, 2, skip1<tag2<block<N, BN, 2, tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<con<N, 3, 3, 1, 1, relu<BN<con<N, 3, 3, stride, stride, SUBNET>>>>>;

template <int N, typename SUBNET> using ares = relu<residual<block, N, affine, SUBNET>>;
template <int N, typename SUBNET> using ares_down = relu<residual_down<block, N, affine, SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256, SUBNET>;
template <typename SUBNET> using alevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

using anet_type = loss_metric<fc_no_bias<128, avg_pool_everything<
	alevel0<
	alevel1<
	alevel2<
	alevel3<
	alevel4<
	max_pool<3, 3, 2, 2, relu<affine<con<32, 7, 7, 2, 2,
	input_rgb_image_sized<150>
	>>>>>>>>>>>>;

// ----------------------------------------------------------------------------------------

std::vector<matrix<rgb_pixel>> jitter_image(
	const matrix<rgb_pixel>& img
);

int main(int argc, char** argv)try
{
	cv::VideoCapture capture;
	capture.open(0);
	if (!capture.isOpened()) {
		cout << "Open Camera Failed!" << endl;
		return 0;
	}

	image_window win;

	frontal_face_detector detector = get_frontal_face_detector();
	shape_predictor sp;
	deserialize("shape_predictor_5_face_landmarks.dat") >> sp;
	anet_type net;
	deserialize("dlib_face_recognition_resnet_model_v1.dat") >> net;
	
	matrix<rgb_pixel> imgCMP;
	load_image(imgCMP, "jiang.jpg");
	std::vector<matrix<rgb_pixel>> CMPfaces;
	for (auto face : detector(imgCMP))
	{
		auto shape = sp(imgCMP, face);
		matrix<rgb_pixel> face_chip;
		extract_image_chip(imgCMP, get_face_chip_details(shape, 150, 0.25), face_chip);
		CMPfaces.push_back(move(face_chip));
	}

	std::vector<matrix<float, 0, 1>> compareDescriptors = net(CMPfaces);
	
	while (!win.is_closed())
	{
		cv::Mat frame;
		cv::Mat small_frame;
		capture >> frame;

		int height = frame.rows;
		int width = frame.cols;
		float fx = 0.7;
		float fy = 0.7;
		cv::Size dsize = cv::Size(round(fx*width), round(fy*height));
		cv::resize(frame, small_frame, dsize, 0, 0, CV_INTER_AREA);

		matrix<rgb_pixel> img;
		assign_image(img, cv_image<rgb_pixel>(small_frame));

		matrix<bgr_pixel> imgShow;
		assign_image(imgShow, cv_image<bgr_pixel>(frame));

		win.set_image(imgShow);
        
		std::vector<rectangle> locations;
		std::vector<matrix<rgb_pixel>> faces;
		for (auto face : detector(img))
		{
			auto shape = sp(img, face);
			matrix<rgb_pixel> face_chip;
			extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_chip);
			faces.push_back(move(face_chip));
			face.left() = face.left() / 0.7;
			face.right() = face.right() / 0.7;
			face.top() = face.top() / 0.7;
			face.bottom() = face.bottom() / 0.7;
			locations.push_back(face);
			win.add_overlay(face);
		}		
		
		std::vector<matrix<float, 0, 1>> face_descriptors = net(faces);
		for (int i = 0; i < face_descriptors.size(); ++i)
		{
			if (length(face_descriptors[i] - compareDescriptors[0]) < 0.6) 
			{
				rectangle det;
				det.left() = locations[i].left();
				det.top() = locations[i].bottom()+6;
				det.right() = locations[i].left()+6;
				det.bottom() = locations[i].bottom();
				win.add_overlay(dlib::image_window::overlay_rect(det,rgb_pixel(255,0,0),"jiang"));
			}
		}
		sleep(200);
		win.clear_overlay();
	}
}
catch (const std::exception&)
{

}

std::vector<matrix<rgb_pixel>> jitter_image(const matrix<rgb_pixel>& img)
{
	thread_local dlib::rand rnd;
	std::vector<matrix<rgb_pixel>> crops;
	for (int i = 0; i < 100; ++i)
		crops.push_back(jitter_image(img, rnd));
	return crops;
}