/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraControlImpl.h"
#include "base/basictypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/unused.h"
#include "nsIWeakReferenceUtils.h"
#include "CameraRecorderProfiles.h"
#include "CameraCommon.h"
#include "nsGlobalWindow.h"
#include "DeviceStorageFileDescriptor.h"
#include "CameraControlListener.h"

using namespace mozilla;

nsWeakPtr CameraControlImpl::sCameraThread;

CameraControlImpl::CameraControlImpl(uint32_t aCameraId)
  : mCameraId(aCameraId)
  , mPreviewState(CameraControlListener::kPreviewStopped)
  , mHardwareState(CameraControlListener::kHardwareClosed)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  // reuse the same camera thread to conserve resources
  nsCOMPtr<nsIThread> ct = do_QueryReferent(sCameraThread);
  if (ct) {
    mCameraThread = ct.forget();
  } else {
    nsresult rv = NS_NewNamedThread("CameraThread", getter_AddRefs(mCameraThread));
    unused << rv; // swallow rv to suppress a compiler warning when the macro
                  // is #defined to nothing (i.e. in non-DEBUG builds).
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // keep a weak reference to the new thread
    sCameraThread = do_GetWeakReference(mCameraThread);
  }

  // Care must be taken with the mListenerLock read-write lock to prevent
  // deadlocks. Currently this is handled by ensuring that any attempts to
  // acquire the lock for writing (as in Add/RemoveListener()) happen in a
  // runnable dispatched to the Camera Thread--even if the method is being
  // called from that thread. This ensures that if a registered listener
  // (which is invoked with a read-lock) tries to call Add/RemoveListener(),
  // the lock-for-writing attempt won't happen until the listener has
  // completed.
  //
  // Multiple parallel listeners being invoked are not a problem because
  // the read-write lock allows multiple simultaneous read-locks.
  mListenerLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "CameraControlImpl.Listeners.Lock");
}

CameraControlImpl::~CameraControlImpl()
{
  if (mListenerLock) {
    PR_DestroyRWLock(mListenerLock);
    mListenerLock = nullptr;
  }
}

already_AddRefed<RecorderProfileManager>
CameraControlImpl::GetRecorderProfileManager()
{
  return GetRecorderProfileManagerImpl();
}

void
CameraControlImpl::Shutdown()
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
}

void
CameraControlImpl::OnHardwareStateChange(CameraControlListener::HardwareState aNewState)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it may be called from the camera's
  //  local binder thread, should the mediaserver process die.
  RwLockAutoEnterRead lock(mListenerLock);

  if (aNewState == mHardwareState) {
    DOM_CAMERA_LOGI("OnHardwareStateChange: state did not change from %d\n", mHardwareState);
    return;
  }

#ifdef PR_LOGGING
  const char* state[] = { "open", "closed", "failed" };
  MOZ_ASSERT(aNewState >= 0);
  if (static_cast<unsigned int>(aNewState) < sizeof(state) / sizeof(state[0])) {
    DOM_CAMERA_LOGI("New hardware state is '%s'\n", state[aNewState]);
  } else {
    DOM_CAMERA_LOGE("OnHardwareStateChange: got invalid HardwareState value %d\n", aNewState);
  }
#endif

  mHardwareState = aNewState;

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnHardwareStateChange(mHardwareState);
  }
}

void
CameraControlImpl::OnConfigurationChange()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RwLockAutoEnterRead lock(mListenerLock);

  DOM_CAMERA_LOGI("OnConfigurationChange : %d listeners\n", mListeners.Length());

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnConfigurationChange(mCurrentConfiguration);
  }
}

void
CameraControlImpl::OnAutoFocusComplete(bool aAutoFocusSucceeded)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it is called from the camera
  //  library's auto focus thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnAutoFocusComplete(aAutoFocusSucceeded);
  }
}

void
CameraControlImpl::OnAutoFocusMoving(bool aIsMoving)
{
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnAutoFocusMoving(aIsMoving);
  }
}

void
CameraControlImpl::OnFacesDetected(const nsTArray<Face>& aFaces)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it is called from the camera
  //  library's face detection thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnFacesDetected(aFaces);
  }
}

void
CameraControlImpl::OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it is called from the camera
  //  library's snapshot thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnTakePictureComplete(aData, aLength, aMimeType);
  }
}

void
CameraControlImpl::OnShutter()
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it is called from the camera driver's
  //  preview thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnShutter();
  }
}

void
CameraControlImpl::OnClosed()
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnHardwareStateChange(CameraControlListener::kHardwareClosed);
  }
}

void
CameraControlImpl::OnRecorderStateChange(CameraControlListener::RecorderState aState,
                                         int32_t aStatus, int32_t aTrackNumber)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread. On Gonk, it is called from the media encoder
  //  thread.
  RwLockAutoEnterRead lock(mListenerLock);

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnRecorderStateChange(aState, aStatus, aTrackNumber);
  }
}

void
CameraControlImpl::OnPreviewStateChange(CameraControlListener::PreviewState aNewState)
{
  // This callback runs on the Main Thread and the Camera Thread, and
  //  may run on the local binder thread, should the mediaserver
  //  process die.
  RwLockAutoEnterRead lock(mListenerLock);

  if (aNewState == mPreviewState) {
    DOM_CAMERA_LOGI("OnPreviewStateChange: state did not change from %d\n", mPreviewState);
    return;
  }

#ifdef PR_LOGGING
  const char* state[] = { "stopped", "paused", "started" };
  MOZ_ASSERT(aNewState >= 0);
  if (static_cast<unsigned int>(aNewState) < sizeof(state) / sizeof(state[0])) {
    DOM_CAMERA_LOGI("New preview state is '%s'\n", state[aNewState]);
  } else {
    DOM_CAMERA_LOGE("OnPreviewStateChange: got unknown PreviewState value %d\n", aNewState);
  }
#endif

  mPreviewState = aNewState;

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnPreviewStateChange(mPreviewState);
  }
}

bool
CameraControlImpl::OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight)
{
  // This function runs on neither the Main Thread nor the Camera Thread.
  //  On Gonk, it is called from the camera driver's preview thread.
  RwLockAutoEnterRead lock(mListenerLock);

  DOM_CAMERA_LOGI("OnNewPreviewFrame: we have %d preview frame listener(s)\n",
    mListeners.Length());

  bool consumed = false;

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    consumed = l->OnNewPreviewFrame(aImage, aWidth, aHeight) || consumed;
  }
  return consumed;
}

void
CameraControlImpl::OnError(CameraControlListener::CameraErrorContext aContext,
                           CameraControlListener::CameraError aError)
{
  // This callback can run on threads other than the Main Thread and
  //  the Camera Thread.
  RwLockAutoEnterRead lock(mListenerLock);

#ifdef PR_LOGGING
  const char* error[] = {
    "api-failed",
    "init-failed",
    "invalid-configuration",
    "service-failed",
    "set-picture-size-failed",
    "set-thumbnail-size-failed",
    "unknown"
  };
  const char* context[] = {
    "StartCamera",
    "StopCamera",
    "AutoFocus",
    "StartFaceDetection",
    "StopFaceDetection",
    "TakePicture",
    "StartRecording",
    "StopRecording",
    "SetConfiguration",
    "StartPreview",
    "StopPreview",
    "ResumeContinuousFocus",
    "Unspecified"
  };
  if (static_cast<unsigned int>(aError) < sizeof(error) / sizeof(error[0]) &&
    static_cast<unsigned int>(aContext) < sizeof(context) / sizeof(context[0])) {
    DOM_CAMERA_LOGW("CameraControlImpl::OnError : aContext='%s' (%u), aError='%s' (%u)\n",
      context[aContext], aContext, error[aError], aError);
  } else {
    DOM_CAMERA_LOGE("CameraControlImpl::OnError : aContext=%u, aError=%d\n",
      aContext, aError);
  }
#endif

  for (uint32_t i = 0; i < mListeners.Length(); ++i) {
    CameraControlListener* l = mListeners[i];
    l->OnError(aContext, aError);
  }
}

// Camera control asynchronous message; these are dispatched from
//  the Main Thread to the Camera Thread, where they are consumed.

class CameraControlImpl::ControlMessage : public nsRunnable
{
public:
  ControlMessage(CameraControlImpl* aCameraControl,
                 CameraControlListener::CameraErrorContext aContext)
    : mCameraControl(aCameraControl)
    , mContext(aContext)
  {
    MOZ_COUNT_CTOR(CameraControlImpl::ControlMessage);
  }

  virtual ~ControlMessage()
  {
    MOZ_COUNT_DTOR(CameraControlImpl::ControlMessage);
  }

  virtual nsresult RunImpl() = 0;

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mCameraControl);
    MOZ_ASSERT(NS_GetCurrentThread() == mCameraControl->mCameraThread);

    nsresult rv = RunImpl();
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGW("Camera control API failed at %d with 0x%x\n", mContext, rv);
      // XXXmikeh - do we want to report a more specific error code?
      mCameraControl->OnError(mContext, CameraControlListener::kErrorApiFailed);
    }

    return NS_OK;
  }

protected:
  nsRefPtr<CameraControlImpl> mCameraControl;
  CameraControlListener::CameraErrorContext mContext;
};

nsresult
CameraControlImpl::Start(const Configuration* aConfig)
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext,
            const Configuration* aConfig)
      : ControlMessage(aCameraControl, aContext)
      , mHaveInitialConfig(false)
    {
      if (aConfig) {
        mConfig = *aConfig;
        mHaveInitialConfig = true;
      }
    }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      if (mHaveInitialConfig) {
        return mCameraControl->StartImpl(&mConfig);
      }
      return mCameraControl->StartImpl();
    }

  protected:
    bool mHaveInitialConfig;
    Configuration mConfig;
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStartCamera, aConfig), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::SetConfiguration(const Configuration& aConfig)
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext,
            const Configuration& aConfig)
      : ControlMessage(aCameraControl, aContext)
      , mConfig(aConfig)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->SetConfigurationImpl(mConfig);
    }

  protected:
    Configuration mConfig;
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInSetConfiguration, aConfig), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::AutoFocus()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->AutoFocusImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInAutoFocus), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StartFaceDetection()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StartFaceDetectionImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStartFaceDetection), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StopFaceDetection()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StopFaceDetectionImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStopFaceDetection), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::TakePicture()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->TakePictureImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInTakePicture), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                  const StartRecordingOptions* aOptions)
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext,
            const StartRecordingOptions* aOptions,
            DeviceStorageFileDescriptor* aFileDescriptor)
      : ControlMessage(aCameraControl, aContext)
      , mOptionsPassed(false)
      , mFileDescriptor(aFileDescriptor)
    {
      if (aOptions) {
        mOptions = *aOptions;
        mOptionsPassed = true;
      }
    }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StartRecordingImpl(mFileDescriptor,
        mOptionsPassed ? &mOptions : nullptr);
    }

  protected:
    StartRecordingOptions mOptions;
    bool mOptionsPassed;
    nsRefPtr<DeviceStorageFileDescriptor> mFileDescriptor;
  };


  return mCameraThread->Dispatch(new Message(this, CameraControlListener::kInStartRecording,
    aOptions, aFileDescriptor), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StopRecording()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StopRecordingImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStopRecording), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StartPreview()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StartPreviewImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStartPreview), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StopPreview()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StopPreviewImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStopPreview), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::ResumeContinuousFocus()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->ResumeContinuousFocusImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInResumeContinuousFocus), NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::Stop()
{
  class Message : public ControlMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener::CameraErrorContext aContext)
      : ControlMessage(aCameraControl, aContext)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      return mCameraControl->StopImpl();
    }
  };

  return mCameraThread->Dispatch(
    new Message(this, CameraControlListener::kInStopCamera), NS_DISPATCH_NORMAL);
}

class CameraControlImpl::ListenerMessage : public CameraControlImpl::ControlMessage
{
public:
  ListenerMessage(CameraControlImpl* aCameraControl,
                  CameraControlListener* aListener)
    : ControlMessage(aCameraControl, CameraControlListener::kInUnspecified)
    , mListener(aListener)
  { }

protected:
  nsRefPtr<CameraControlListener> mListener;
};

void
CameraControlImpl::AddListenerImpl(already_AddRefed<CameraControlListener> aListener)
{
  RwLockAutoEnterWrite lock(mListenerLock);

  CameraControlListener* l = *mListeners.AppendElement() = aListener;
  DOM_CAMERA_LOGI("Added camera control listener %p\n", l);

  // Update the newly-added listener's state
  l->OnConfigurationChange(mCurrentConfiguration);
  l->OnHardwareStateChange(mHardwareState);
  l->OnPreviewStateChange(mPreviewState);
}

void
CameraControlImpl::AddListener(CameraControlListener* aListener)
 {
  class Message : public ListenerMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl,
            CameraControlListener* aListener)
      : ListenerMessage(aCameraControl, aListener)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      mCameraControl->AddListenerImpl(mListener.forget());
      return NS_OK;
    }
  };

  mCameraThread->Dispatch(new Message(this, aListener), NS_DISPATCH_NORMAL);
}

void
CameraControlImpl::RemoveListenerImpl(CameraControlListener* aListener)
{
  RwLockAutoEnterWrite lock(mListenerLock);

  nsRefPtr<CameraControlListener> l(aListener);
  mListeners.RemoveElement(l);
  DOM_CAMERA_LOGI("Removed camera control listener %p\n", l.get());
  // XXXmikeh - do we want to notify the listener that it has been removed?
}

void
CameraControlImpl::RemoveListener(CameraControlListener* aListener)
 {
  class Message : public ListenerMessage
  {
  public:
    Message(CameraControlImpl* aCameraControl, CameraControlListener* aListener)
      : ListenerMessage(aCameraControl, aListener)
    { }

    nsresult
    RunImpl() MOZ_OVERRIDE
    {
      mCameraControl->RemoveListenerImpl(mListener);
      return NS_OK;
    }
  };

  mCameraThread->Dispatch(new Message(this, aListener), NS_DISPATCH_NORMAL);
}
