/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraControlListener.h"
#include "nsThreadUtils.h"
#include "nsDOMFile.h"
#include "CameraCommon.h"
#include "DOMCameraControl.h"
#include "CameraPreviewMediaStream.h"
#include "mozilla/dom/CameraManagerBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

DOMCameraControlListener::DOMCameraControlListener(nsDOMCameraControl* aDOMCameraControl,
                                                   CameraPreviewMediaStream* aStream)
  : mDOMCameraControl(new nsMainThreadPtrHolder<nsDOMCameraControl>(aDOMCameraControl))
  , mStream(aStream)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, camera=%p, stream=%p\n",
    __func__, __LINE__, this, aDOMCameraControl, aStream);
}

DOMCameraControlListener::~DOMCameraControlListener()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

// Boilerplate callback runnable
class DOMCameraControlListener::DOMCallback : public nsRunnable
{
public:
  DOMCallback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl)
    : mDOMCameraControl(aDOMCameraControl)
  {
    MOZ_COUNT_CTOR(DOMCameraControlListener::DOMCallback);
  }
  virtual ~DOMCallback()
  {
    MOZ_COUNT_DTOR(DOMCameraControlListener::DOMCallback);
  }

  virtual void RunCallback(nsDOMCameraControl* aDOMCameraControl) = 0;

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<nsDOMCameraControl> camera = mDOMCameraControl.get();
    if (camera) {
      RunCallback(camera);
    }
    return NS_OK;
  }

protected:
  nsMainThreadPtrHandle<nsDOMCameraControl> mDOMCameraControl;
};

// Specific callback handlers
void
DOMCameraControlListener::OnHardwareStateChange(HardwareState aState)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             HardwareState aState)
      : DOMCallback(aDOMCameraControl)
      , mState(aState)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      aDOMCameraControl->OnHardwareStateChange(mState);
    }

  protected:
    HardwareState mState;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aState));
}

void
DOMCameraControlListener::OnPreviewStateChange(PreviewState aState)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             PreviewState aState)
      : DOMCallback(aDOMCameraControl)
      , mState(aState)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      aDOMCameraControl->OnPreviewStateChange(mState);
    }

  protected:
    PreviewState mState;
  };

  switch (aState) {
    case kPreviewStopped:
      // Clear the current frame right away, without dispatching a
      //  runnable. This is an ugly coupling between the camera's
      //  SurfaceTextureClient and the MediaStream/ImageContainer,
      //  but without it, the preview can fail to start.
      DOM_CAMERA_LOGI("Preview stopped, clearing current frame\n");
      mStream->ClearCurrentFrame();
      break;

    case kPreviewPaused:
      // In the paused state, we still want to reflect the change
      //  in preview state, but we don't want to clear the current
      //  frame as above, since doing so seems to cause genlock
      //  problems when we restart the preview. See bug 957749.
      DOM_CAMERA_LOGI("Preview paused\n");
      break;

    case kPreviewStarted:
      DOM_CAMERA_LOGI("Preview started\n");
      break;

    default:
      DOM_CAMERA_LOGE("Unknown preview state %d\n", aState);
      MOZ_ASSUME_UNREACHABLE("Invalid preview state");
      return;
  }
  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aState));
}

void
DOMCameraControlListener::OnRecorderStateChange(RecorderState aState,
                                                int32_t aStatus, int32_t aTrackNum)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             RecorderState aState,
             int32_t aStatus,
             int32_t aTrackNum)
      : DOMCallback(aDOMCameraControl)
      , mState(aState)
      , mStatus(aStatus)
      , mTrackNum(aTrackNum)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      aDOMCameraControl->OnRecorderStateChange(mState, mStatus, mTrackNum);
    }

  protected:
    RecorderState mState;
    int32_t mStatus;
    int32_t mTrackNum;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aState, aStatus, aTrackNum));
}

void
DOMCameraControlListener::OnConfigurationChange(const CameraListenerConfiguration& aConfiguration)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             const CameraListenerConfiguration& aConfiguration)
      : DOMCallback(aDOMCameraControl)
      , mConfiguration(aConfiguration)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      nsRefPtr<nsDOMCameraControl::DOMCameraConfiguration> config =
        new nsDOMCameraControl::DOMCameraConfiguration();

      switch (mConfiguration.mMode) {
        case ICameraControl::kVideoMode:
          config->mMode = CameraMode::Video;
          break;

        case ICameraControl::kPictureMode:
          config->mMode = CameraMode::Picture;
          break;

        default:
          DOM_CAMERA_LOGI("Camera mode still unspecified, nothing to do\n");
          return;
      }

      // Map CameraControl parameters to their DOM-facing equivalents
      config->mRecorderProfile = mConfiguration.mRecorderProfile;
      config->mPreviewSize.mWidth = mConfiguration.mPreviewSize.width;
      config->mPreviewSize.mHeight = mConfiguration.mPreviewSize.height;
      config->mMaxMeteringAreas = mConfiguration.mMaxMeteringAreas;
      config->mMaxFocusAreas = mConfiguration.mMaxFocusAreas;

      aDOMCameraControl->OnConfigurationChange(config);
    }

  protected:
    const CameraListenerConfiguration mConfiguration;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aConfiguration));
}

void
DOMCameraControlListener::OnShutter()
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl)
      : DOMCallback(aDOMCameraControl)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      aDOMCameraControl->OnShutter();
    }
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl));
}

bool
DOMCameraControlListener::OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight)
{
  DOM_CAMERA_LOGI("OnNewPreviewFrame: got %d x %d frame\n", aWidth, aHeight);

  mStream->SetCurrentFrame(gfxIntSize(aWidth, aHeight), aImage);
  return true;
}

void
DOMCameraControlListener::OnAutoFocusComplete(bool aAutoFocusSucceeded)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             bool aAutoFocusSucceeded)
      : DOMCallback(aDOMCameraControl)
      , mAutoFocusSucceeded(aAutoFocusSucceeded)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      aDOMCameraControl->OnAutoFocusComplete(mAutoFocusSucceeded);
    }

  protected:
    bool mAutoFocusSucceeded;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aAutoFocusSucceeded));
}

void
DOMCameraControlListener::OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             uint8_t* aData, uint32_t aLength, const nsAString& aMimeType)
      : DOMCallback(aDOMCameraControl)
      , mData(aData)
      , mLength(aLength)
      , mMimeType(aMimeType)
    { }

    void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      nsCOMPtr<nsIDOMBlob> picture = new nsDOMMemoryFile(static_cast<void*>(mData),
                                                         static_cast<uint64_t>(mLength),
                                                         mMimeType);
      aDOMCameraControl->OnTakePictureComplete(picture);
    }

  protected:
    uint8_t* mData;
    uint32_t mLength;
    nsString mMimeType;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aData, aLength, aMimeType));
}

void
DOMCameraControlListener::OnError(CameraErrorContext aContext, CameraError aError)
{
  class Callback : public DOMCallback
  {
  public:
    Callback(nsMainThreadPtrHandle<nsDOMCameraControl> aDOMCameraControl,
             CameraErrorContext aContext,
             CameraError aError)
      : DOMCallback(aDOMCameraControl)
      , mContext(aContext)
      , mError(aError)
    { }

    virtual void
    RunCallback(nsDOMCameraControl* aDOMCameraControl) MOZ_OVERRIDE
    {
      nsString error;

      switch (mError) {
        case kErrorServiceFailed:
          error = NS_LITERAL_STRING("ErrorServiceFailed");
          break;

        case kErrorSetPictureSizeFailed:
          error = NS_LITERAL_STRING("ErrorSetPictureSizeFailed");
          break;

        case kErrorSetThumbnailSizeFailed:
          error = NS_LITERAL_STRING("ErrorSetThumbnailSizeFailed");
          break;

        case kErrorApiFailed:
          // XXXmikeh legacy error placeholder
          error = NS_LITERAL_STRING("FAILURE");
          break;

        default:
          error = NS_LITERAL_STRING("ErrorUnknown");
          break;
      }
      aDOMCameraControl->OnError(mContext, error);
    }

  protected:
    CameraErrorContext mContext;
    CameraError mError;
  };

  NS_DispatchToMainThread(new Callback(mDOMCameraControl, aContext, aError));
}
