#include "targetTracking.h"
Rect g_selection;
bool g_selectObject = false;
Point g_origin;
Mat g_image = Mat::zeros(640, 640, CV_8UC3);
int g_trackObject = 0 ;
void TargetTracking::test()
{
    int x = 0;
    for(int i = 0;i<1000;i++)
    {

      ptz_command_->PTZcontrol(Point(320,240),Point(x,240),2);
      x = x+100;
      if(x>640)
          x = 0;
      waitKey(1000);
    }

}
static void onMouse(int event, int x, int y, int, void*)  //用于鼠标选取目标的回调函数
{
    if (g_selectObject)
    {
        g_selection.x = MIN(x, g_origin.x);
        g_selection.y = MIN(y, g_origin.y);
        g_selection.width = std::abs(x - g_origin.x);
        g_selection.height = std::abs(y - g_origin.y);

        g_selection &= Rect(0, 0, g_image.cols, g_image.rows);
       // frameNum = -1;
       // minWidth = selection.width;
       //minHeight = selection.height;
    }

    switch (event)
    {
    case CV_EVENT_LBUTTONDOWN:
        g_origin = Point(x, y);
        g_selection = Rect(x, y, 0, 0);
        g_selectObject = true;
        break;
    case CV_EVENT_LBUTTONUP:
        g_selectObject = false;
        if (g_selection.width > 0 && g_selection.height > 0)
            g_trackObject = -1;
        break;
    }
}

TargetTracking::TargetTracking()
{
    backproj_mode_ = false;
    show_hist_ = true;
    vmin_ = 10 ;
    vmax_ = 256;
    smin_ = 30;
    ptz_command_ = new PTZCommand;
    old_point_ = Point(320,240);
    exit_flag_ = false;

}
TargetTracking::~TargetTracking()
{
    delete ptz_command_;
}

void TargetTracking::DrawCross( Point center, Scalar color,int d )
{
  line(  g_image, Point( center.x - d, center.y - d ),
    Point( center.x + d, center.y + d ), color, 2, CV_AA, 0);
  line(  g_image, Point( center.x + d, center.y - d ),
    Point( center.x - d, center.y + d ), color, 2, CV_AA, 0 );
}

int TargetTracking:: iAbsolute(int a, int b)
{
    int c = 0;
    if (a > b)
    {
        c = a - b;
    }
    else
    {
        c = b - a;
    }
    return	c;
}

void TargetTracking::set_exit_flag()
{
    if(exit_flag_ == false)
      exit_flag_ =true;

}

int TargetTracking::tracking()
{
    VideoCapture cap;
    Rect trackWindow;
    int hsize = 80;  //越大越准确，同时计算量也越大
    float hranges[] = { 0, 180 };
    const float* phranges = hranges;

  //  CommandLineParser parser(argc, argv, keys);
    /*************粒子滤波初始化************************************/
    Mat_<float> measurement(2,1);
    measurement.setTo(cv::Scalar(0));
    int dim = 2;
    int nParticles = 200;
    float x_range = 640.0;
    float y_range = 480.0;

    float min_range[] = { 0, 0 };
    float max_range[] = { x_range, y_range };
    CvMat LB, UB;
    cvInitMatHeader(&LB, 2, 1, CV_32FC1, min_range);
    cvInitMatHeader(&UB, 2, 1, CV_32FC1, max_range);

    CvConDensation* condens = cvCreateConDensation(dim, dim, nParticles);

    cvConDensInitSampleSet(condens, &LB, &UB);
    condens->DynamMatr[0] = 1.0;
    condens->DynamMatr[1] = 0.0;
    condens->DynamMatr[2] = 0.0;
    condens->DynamMatr[3] = 1.0;
    cap.open(0);//打开摄像头
    if (!cap.isOpened())
    {
        //help();
        cout << "***Could not initialize capturing...***\n";
        cout << "Current parameter's value: \n";
        //parser.printParams();
        return -1;
    }

    namedWindow("Histogram", 0);
    namedWindow("TargetTracking", 0);
    setMouseCallback("TargetTracking", onMouse, 0);
  //  createTrackbar("Vmin", "CamShift Demo", &vmin, 256, 0);
 //   createTrackbar("Vmax", "CamShift Demo", &vmax, 256, 0);
  //  createTrackbar("Smin", "CamShift Demo", &smin, 256, 0);

    Mat frame = Mat::zeros(640, 480, CV_8UC3);;
    Mat hsv = Mat::zeros(640, 480, CV_8UC3);;
    Mat hue = Mat::zeros(640, 480, CV_8UC3);;
    Mat mask = Mat::zeros(640, 480, CV_8UC3);;
    Mat hist = Mat::zeros(640, 480, CV_8UC3);;
    Mat histimg = Mat::zeros(640, 480, CV_8UC3);
    Mat backproj;
    bool paused = false;
    int frame_num = 0;
    for (;;)
    {

        if (!paused)
        {
            cap >> frame;
            if (frame.empty())
                break;
        }
        //if (frameNUm == 50)
        //	waitKey(0)
        //frame.copyTo(image);
        GaussianBlur(frame,g_image,Size(1,1),0,0);


        if (!paused)
        {
            cvtColor(g_image, hsv, COLOR_BGR2HSV);//转换到hsv空间

            if (g_trackObject)//已经选择好跟踪对象
            {
                frame_num++;
                int _vmin = vmin_, _vmax = vmax_;

                inRange(hsv, Scalar(0, smin_, MIN(_vmin, _vmax)),
                    Scalar(180, 256, MAX(_vmin, _vmax)), mask);
                imshow("mask", mask);
                int ch[] = { 0, 0 };
                hue.create(hsv.size(), hsv.depth());
                mixChannels(&hsv, 1, &hue, 1, ch, 1);
                //提取出h 分量
                if (g_trackObject < 0)//计算选择目标的内的特征
                {
                    Mat roi(hue, g_selection), maskroi(mask, g_selection);
                    calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);//计算直方图
                    normalize(hist, hist, 0, 255, CV_MINMAX);//直方图归一化

                    trackWindow = g_selection;
                    g_trackObject = 1;

                    histimg = Scalar::all(0);
                    int binW = histimg.cols / hsize;
                    Mat buf(1, hsize, CV_8UC3);
                    for (int i = 0; i < hsize; i++)
                        buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
                    cvtColor(buf, buf, CV_HSV2BGR);//画出直方图

                    for (int i = 0; i < hsize; i++)
                    {
                        int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
                        rectangle(histimg, Point(i*binW, histimg.rows),
                            Point((i + 1)*binW, histimg.rows - val),
                            Scalar(buf.at<Vec3b>(i)), -1, 8);
                    }
                }

                calcBackProject(&hue, 1, 0, hist, backproj, &phranges);//计算反向投影图
                backproj &= mask;
                RotatedRect trackBox = CamShift(backproj, trackWindow,
                TermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1));//camshift主函数，最后一个参数是迭代停止的标准
                cout<<trackBox.center.x<<" "<<trackBox.center.y<<endl;
                cout<<trackWindow.x<<" "<<trackWindow.y<<endl;
                //if(frameNum = -1 )
                  //  oldPoint = trackBox.center;
               // frameNum = frameNum + 2;

                 ptz_command_->PTZcontrol(old_point_,trackBox.center,frame_num);

                //oldPoint = trackBox.center;
                /**********粒子滤波及其更新**********************/
                measurement(0)=trackBox.center.x;
                measurement(1)=trackBox.center.y;
                for (int i = 0; i < condens->SamplesNum; i++)
                {

                   float diffX = (measurement(0) - condens->flSamples[i][0])/x_range;
                   float diffY = (measurement(1) - condens->flSamples[i][1])/y_range;
                   condens->flConfidence[i] = 1.0 / (sqrt(diffX * diffX + diffY * diffY));
                   Point partPt(condens->flSamples[i][0], condens->flSamples[i][1]);
                   DrawCross(partPt , Scalar(255,0,255), 2);
                }
                cvConDensUpdateByTime(condens);
                Point statePt(condens->State[0], condens->State[1]);
                circle( g_image,statePt,5,CV_RGB(0,0,255),3);
                circle( g_image,trackBox.center,5,CV_RGB(0,255,0),3);
                circle( g_image,Point(320,240),5,CV_RGB(255,0,0),3);


                if (trackWindow.area() <= 10)
                {
//                    int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
//                    trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
//                        trackWindow.x + r, trackWindow.y + r) &
//                        Rect(0, 0, cols, rows);//赋值给下一次查找的区域
                     trackWindow = Rect(statePt.x,statePt.y,100,100)&Rect(0,0,640,640);
                }
//                int iBetween = 0;
//                        //确保预测点 与 实际点之间 连线距离 在 本次 trackBox 的size 之内
//                 iBetween = sqrt(powf((statePt.x - trackBox.center.x),2)
//                            +
//                            powf((statePt.y - trackBox.center.y),2) );

//                        Point prePoint;//预测的点 相对于 实际点 的对称点



//                        if ( iBetween > 5)
//                        {
//                            //当实际点 在 预测点 右边
//                            if (trackBox.center.x > statePt.x)
//                            {
//                                //且，实际点在 预测点 下面
//                                if (trackBox.center.y > statePt.y)
//                                {
//                                    prePoint.x = trackBox.center.x + iAbsolute(trackBox.center.x,statePt.x);
//                                    prePoint.y = trackBox.center.y + iAbsolute(trackBox.center.y,statePt.y);
//                                }
//                                //且，实际点在 预测点 上面
//                                else
//                                {
//                                    prePoint.x = trackBox.center.x + iAbsolute(trackBox.center.x,statePt.x);
//                                    prePoint.y = trackBox.center.y - iAbsolute(trackBox.center.y,statePt.y);
//                                }
//                                //宽高
//                                if (trackWindow.width != 0)
//                                {
//                                    trackWindow.width += iBetween + iAbsolute(trackBox.center.x,statePt.x);
//                                }

//                                if (trackWindow.height != 0)
//                                {
//                                    trackWindow.height += iBetween + iAbsolute(trackBox.center.x,statePt.x);
//                                }
//                            }
//                            //当实际点 在 预测点 左边
//                            else
//                            {
//                                //且，实际点在 预测点 下面
//                                if (trackBox.center.y > statePt.y)
//                                {
//                                    prePoint.x = trackBox.center.x - iAbsolute(trackBox.center.x,statePt.x);
//                                    prePoint.y = trackBox.center.y + iAbsolute(trackBox.center.y,statePt.y);
//                                }
//                                //且，实际点在 预测点 上面
//                                else
//                                {
//                                    prePoint.x = trackBox.center.x - iAbsolute(trackBox.center.x,statePt.x);
//                                    prePoint.y = trackBox.center.y - iAbsolute(trackBox.center.y,statePt.y);
//                                }
//                                //宽高
//                                if (trackWindow.width != 0)
//                                {
//                                    trackWindow.width += iBetween + iAbsolute(trackBox.center.x,statePt.x);
//                                }

//                                if (trackWindow.height != 0)
//                                {
//                                    trackWindow.height += iBetween +iAbsolute(trackBox.center.x,statePt.x);
//                                }
//                            }

//                            trackWindow.x = prePoint.x - iBetween;
//                            trackWindow.y = prePoint.y - iBetween;
//                        }
//                        else
//                        {
//                            trackWindow.x -= iBetween;
//                            trackWindow.y -= iBetween;
//                            //宽高
//                            if (trackWindow.width != 0)
//                            {
//                                trackWindow.width += iBetween;
//                            }

//                            if (trackWindow.height != 0)
//                            {
//                                trackWindow.height += iBetween;
//                            }
//                        }

//                        //跟踪的矩形框不能小于初始化检测到的大小，当这个情况的时候，X 和 Y可以适当的在缩小
//                        if (trackWindow.width < minWidth)
//                        {
//                            trackWindow.width = minWidth;
//                            trackWindow.x -= iBetween;
//                        }
//                        if (trackWindow.height < minHeight)
//                        {
//                            trackWindow.height = minHeight;
//                            trackWindow.y -= iBetween;
//                        }

//                        //确保调整后的矩形大小在640 * 480之内
//                        if (trackWindow.x <= 0)
//                        {
//                            trackWindow.x = 0;
//                        }
//                        if (trackWindow.y <= 0)
//                        {
//                            trackWindow.y = 0;
//                        }
//                        if (trackWindow.x >= 600)
//                        {
//                            trackWindow.x = 600;
//                        }
//                        if (trackWindow.y >= 440)
//                        {
//                            trackWindow.y = 440;
//                        }

//                        if (trackWindow.width + trackWindow.x >= 640)
//                        {
//                            trackWindow.width = 640 - trackWindow.x;
//                        }
//                        if (trackWindow.height + trackWindow.y >= 480)
//                        {
//                            trackWindow.height = 480 - trackWindow.y;
//                        }
                if (backproj_mode_)
                    cvtColor(backproj, g_image, COLOR_GRAY2BGR);
                ellipse(g_image, trackBox, Scalar(0, 0, 255), 3, CV_AA);//画出椭圆目标区域

        }
        else if (g_trackObject < 0)
            paused = false;

        if (g_selectObject && g_selection.width > 0 && g_selection.height > 0)
        {
            Mat roi(g_image, g_selection);
            bitwise_not(roi, roi);//黑白反转
        }


        imshow("TargetTracking", g_image);
        imshow("Histogram", histimg);

        if(exit_flag_ == true)
        {
            exit_flag_ = false;
            break;
        }
        char c = (char)waitKey(3);
        if (c == 27)
            break;
        switch (c)
        {
        case 'b':
            backproj_mode_ = !backproj_mode_;
            break;
        case 'c':
            g_trackObject = 0;
            histimg = Scalar::all(0);
            break;
        case 'h':
            show_hist_ = !show_hist_;
            if (!show_hist_)
                destroyWindow("Histogram");
            else
                namedWindow("Histogram", 1);
            break;
        case 'p':
            paused = !paused;
            break;
        default:
            ;
        }
    }


}
  destroyWindow("TargetTracking");
  destroyWindow("mask");
  destroyWindow("Histogram");
  g_selectObject = false;
  g_trackObject = 0 ;
return 0;
}