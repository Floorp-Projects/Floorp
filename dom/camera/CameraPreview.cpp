/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreview.h"

#define DOM_CAMERA_LOG_LEVEL  3
#include "CameraCommon.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS1(CameraPreview, CameraPreview)

class CameraPreviewListener : public MediaStreamListener
{
public:
  CameraPreviewListener(CameraPreview* aPreview) :
    mPreview(aPreview)
  {
    DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  ~CameraPreviewListener()
  {
    DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  void NotifyConsumptionChanged(MediaStreamGraph* aGraph, Consumption aConsuming)
  {
    const char* state;

    DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);

    switch (aConsuming) {
      case NOT_CONSUMED:
        state = "not consuming";
        break;

      case CONSUMED:
        state = "consuming";
        break;

      default:
        state = "unknown";
        break;
    }

    DOM_CAMERA_LOGA("camera viewfinder is %s\n", state);

    switch (aConsuming) {
      case NOT_CONSUMED:
        mPreview->Stop();
        break;

      case CONSUMED:
        mPreview->Start();
        break;
    }
  }

protected:
  nsCOMPtr<CameraPreview> mPreview;
};

CameraPreview::CameraPreview(nsIThread* aCameraThread, PRUint32 aWidth, PRUint32 aHeight)
  : nsDOMMediaStream()
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mFramesPerSecond(0)
  , mFrameCount(0)
  , mCameraThread(aCameraThread)
{
  DOM_CAMERA_LOGI("%s:%d : mWidth=%d, mHeight=%d : this=%p\n", __func__, __LINE__, mWidth, mHeight, this);

  mImageContainer = LayerManager::CreateImageContainer();
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  mStream = gm->CreateInputStream(this);
  mInput = GetStream()->AsSourceStream();
  mInput->AddListener(new CameraPreviewListener(this));
}

void
CameraPreview::SetFrameRate(PRUint32 aFramesPerSecond)
{
  mFramesPerSecond = aFramesPerSecond;
  mInput->AddTrack(TRACK_VIDEO, mFramesPerSecond, 0, new VideoSegment());
  mInput->AdvanceKnownTracksTime(MEDIA_TIME_MAX);
}

void
CameraPreview::Start()
{
  nsCOMPtr<nsIRunnable> cameraPreviewControl = NS_NewRunnableMethod(this, &CameraPreview::StartImpl);
  nsresult rv = mCameraThread->Dispatch(cameraPreviewControl, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("failed to start camera preview (%d)\n", rv);
  }
}

void
CameraPreview::Stop()
{
  nsCOMPtr<nsIRunnable> cameraPreviewControl = NS_NewRunnableMethod(this, &CameraPreview::StopImpl);
  nsresult rv = mCameraThread->Dispatch(cameraPreviewControl, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("failed to stop camera preview (%d)\n", rv);
  }
}

CameraPreview::~CameraPreview()
{
  DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);
}
