/*
 * Copyright (C) 2013-2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TestGonkCameraHardware.h"
#include "CameraPreferences.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/CameraFacesDetectedEvent.h"
#include "mozilla/dom/CameraStateChangeEvent.h"
#include "nsNetUtil.h"
#include "DOMCameraDetectedFace.h"
#include "nsServiceManagerUtils.h"
#include "nsICameraTestHardware.h"

using namespace android;
using namespace mozilla;
using namespace mozilla::dom;

#ifndef MOZ_WIDGET_GONK
NS_IMPL_ISUPPORTS_INHERITED0(TestGonkCameraHardware, GonkCameraHardware);
#endif

static void
CopyFaceFeature(int32_t (&aDst)[2], bool aExists, const DOMPoint* aSrc)
{
  if (aExists && aSrc) {
    aDst[0] = static_cast<int32_t>(aSrc->X());
    aDst[1] = static_cast<int32_t>(aSrc->Y());
  } else {
    aDst[0] = -2000;
    aDst[1] = -2000;
  }
}

class TestGonkCameraHardwareListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  TestGonkCameraHardwareListener(nsGonkCameraControl* aTarget, nsIThread* aCameraThread)
    : mTarget(aTarget)
    , mCameraThread(aCameraThread)
  {
    MOZ_COUNT_CTOR(TestGonkCameraHardwareListener);
  }

protected:
  virtual ~TestGonkCameraHardwareListener()
  {
    MOZ_COUNT_DTOR(TestGonkCameraHardwareListener);
  }

  nsRefPtr<nsGonkCameraControl> mTarget;
  nsCOMPtr<nsIThread> mCameraThread;
};

NS_IMETHODIMP
TestGonkCameraHardwareListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString eventType;
  aEvent->GetType(eventType);

  DOM_CAMERA_LOGI("Inject '%s' event",
    NS_ConvertUTF16toUTF8(eventType).get());

  if (eventType.EqualsLiteral("focus")) {
    CameraStateChangeEvent* event = aEvent->InternalDOMEvent()->AsCameraStateChangeEvent();

    if (!NS_WARN_IF(!event)) {
      nsString state;

      event->GetNewState(state);
      if (state.EqualsLiteral("focused")) {
        OnAutoFocusComplete(mTarget, true);
      } else if (state.EqualsLiteral("unfocused")) {
        OnAutoFocusComplete(mTarget, false);
      } else if (state.EqualsLiteral("focusing")) {
        OnAutoFocusMoving(mTarget, true);
      } else if (state.EqualsLiteral("not_focusing")) {
        OnAutoFocusMoving(mTarget, false);
      } else {
        DOM_CAMERA_LOGE("Unhandled focus state '%s'\n",
          NS_ConvertUTF16toUTF8(state).get());
      }
    }
  } else if (eventType.EqualsLiteral("shutter")) {
    DOM_CAMERA_LOGI("Inject shutter event");
    OnShutter(mTarget);
  } else if (eventType.EqualsLiteral("picture")) {
    BlobEvent* event = aEvent->InternalDOMEvent()->AsBlobEvent();

    if (!NS_WARN_IF(!event)) {
      Blob* blob = event->GetData();

      if (blob) {
        static const uint64_t MAX_FILE_SIZE = 2147483647;

        ErrorResult rv;
        uint64_t dataLength = blob->GetSize(rv);

        if (NS_WARN_IF(rv.Failed()) || NS_WARN_IF(dataLength > MAX_FILE_SIZE)) {
          rv.SuppressException();
          return NS_OK;
        }

        nsCOMPtr<nsIInputStream> inputStream;
        blob->GetInternalStream(getter_AddRefs(inputStream), rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          return NS_OK;
        }

        uint8_t* data = new uint8_t[dataLength];
        rv = NS_ReadInputStreamToBuffer(inputStream,
                                        reinterpret_cast<void**>(&data),
                                        static_cast<uint32_t>(dataLength));
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          delete [] data;
          return NS_OK;
        }

        OnTakePictureComplete(mTarget, data, dataLength);
        delete [] data;
      } else {
        OnTakePictureComplete(mTarget, nullptr, 0);
      }
    }
  } else if(eventType.EqualsLiteral("error")) {
    ErrorEvent* event = aEvent->InternalDOMEvent()->AsErrorEvent();

    if (!NS_WARN_IF(!event)) {
      nsString errorType;

      event->GetMessage(errorType);
      if (errorType.EqualsLiteral("picture")) {
        OnTakePictureError(mTarget);
      } else if (errorType.EqualsLiteral("system")) {
        if (!NS_WARN_IF(!mCameraThread)) {
          class DeferredSystemFailure : public nsRunnable
          {
          public:
            DeferredSystemFailure(nsGonkCameraControl* aTarget)
              : mTarget(aTarget)
            { }

            NS_IMETHODIMP
            Run()
            {
              OnSystemError(mTarget, CameraControlListener::kSystemService, 100, 0);
              return NS_OK;
            }

          protected:
            nsRefPtr<nsGonkCameraControl> mTarget;
          };

          mCameraThread->Dispatch(new DeferredSystemFailure(mTarget), NS_DISPATCH_NORMAL);
        }
      } else {
        DOM_CAMERA_LOGE("Unhandled error event type '%s'\n",
          NS_ConvertUTF16toUTF8(errorType).get());
      }
    }
  } else if(eventType.EqualsLiteral("facesdetected")) {
    CameraFacesDetectedEvent* event = aEvent->InternalDOMEvent()->AsCameraFacesDetectedEvent();

    if (!NS_WARN_IF(!event)) {
      Nullable<nsTArray<nsRefPtr<DOMCameraDetectedFace>>> faces;
      event->GetFaces(faces);

      camera_frame_metadata_t metadata;
      memset(&metadata, 0, sizeof(metadata));

      if (faces.IsNull()) {
        OnFacesDetected(mTarget, &metadata);
      } else {
        const nsTArray<nsRefPtr<DOMCameraDetectedFace>>& facesData = faces.Value();
        uint32_t i = facesData.Length();

        metadata.number_of_faces = i;
        metadata.faces = new camera_face_t[i];
        memset(metadata.faces, 0, sizeof(camera_face_t) * i);

        while (i > 0) {
          --i;
          const nsRefPtr<DOMCameraDetectedFace>& face = facesData[i];
          camera_face_t& f = metadata.faces[i];
          const DOMRect& bounds = *face->Bounds();
          f.rect[0] = static_cast<int32_t>(bounds.Left());
          f.rect[1] = static_cast<int32_t>(bounds.Top());
          f.rect[2] = static_cast<int32_t>(bounds.Right());
          f.rect[3] = static_cast<int32_t>(bounds.Bottom());
          CopyFaceFeature(f.left_eye, face->HasLeftEye(), face->GetLeftEye());
          CopyFaceFeature(f.right_eye, face->HasRightEye(), face->GetRightEye());
          CopyFaceFeature(f.mouth, face->HasMouth(), face->GetMouth());
          f.id = face->Id();
          f.score = face->Score();
        }

        OnFacesDetected(mTarget, &metadata);
        delete [] metadata.faces;
      }
    }
  } else {
    DOM_CAMERA_LOGE("Unhandled injected event '%s'",
      NS_ConvertUTF16toUTF8(eventType).get());
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(TestGonkCameraHardwareListener, nsIDOMEventListener)

class TestGonkCameraHardware::ControlMessage : public nsRunnable
{
public:
  ControlMessage(TestGonkCameraHardware* aTestHw)
    : mTestHw(aTestHw)
  { }

  NS_IMETHOD
  Run() override
  {
    if (NS_WARN_IF(!mTestHw)) {
      return NS_ERROR_INVALID_ARG;
    }

    MutexAutoLock lock(mTestHw->mMutex);

    mTestHw->mStatus = RunInline();
    nsresult rv = mTestHw->mCondVar.Notify();
    NS_WARN_IF(NS_FAILED(rv));
    return NS_OK;
  }

  nsresult
  RunInline()
  {
    if (NS_WARN_IF(!mTestHw)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsresult rv;
    mJSTestWrapper = do_GetService("@mozilla.org/cameratesthardware;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DOM_CAMERA_LOGE("Cannot get camera test service\n");
      return rv;
    }

    rv = RunImpl();
    mJSTestWrapper = nullptr;
    return rv;
  }

protected:
  NS_IMETHOD RunImpl() = 0;
  virtual ~ControlMessage() { }

  nsCOMPtr<nsICameraTestHardware> mJSTestWrapper;

  /* Since we block the control thread until we have finished
     processing the request on the main thread, we know that this
     pointer will not go out of scope because the control thread
     and calling class is the owner. */
  TestGonkCameraHardware* mTestHw;
};

TestGonkCameraHardware::TestGonkCameraHardware(nsGonkCameraControl* aTarget,
                                               uint32_t aCameraId,
                                               const sp<Camera>& aCamera)
  : GonkCameraHardware(aTarget, aCameraId, aCamera)
  , mMutex("TestGonkCameraHardware::mMutex")
  , mCondVar(mMutex, "TestGonkCameraHardware::mCondVar")
{
  DOM_CAMERA_LOGA("v===== Created TestGonkCameraHardware =====v\n");
  DOM_CAMERA_LOGT("%s:%d : this=%p (aTarget=%p)\n",
    __func__, __LINE__, this, aTarget);
  MOZ_COUNT_CTOR(TestGonkCameraHardware);
  mCameraThread = NS_GetCurrentThread();
}

TestGonkCameraHardware::~TestGonkCameraHardware()
{
  MOZ_COUNT_DTOR(TestGonkCameraHardware);

  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      if (mTestHw->mDomListener) {
        mTestHw->mDomListener = nullptr;
        nsresult rv = mJSTestWrapper->SetHandler(nullptr);
        NS_WARN_IF(NS_FAILED(rv));
      }
      return NS_OK;
    }
  };

  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  NS_WARN_IF(NS_FAILED(rv));
  DOM_CAMERA_LOGA("^===== Destroyed TestGonkCameraHardware =====^\n");
}

nsresult
TestGonkCameraHardware::WaitWhileRunningOnMainThread(nsRefPtr<ControlMessage> aRunnable)
{
  MutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!aRunnable)) {
    mStatus = NS_ERROR_INVALID_ARG;
  } else if (!NS_IsMainThread()) {
    nsresult rv = NS_DispatchToMainThread(aRunnable);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mCondVar.Wait();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    /* Cannot dispatch to main thread since we would block on
       the condvar, so we need to run inline. */
    mStatus = aRunnable->RunInline();
  }
  return mStatus;
}

nsresult
TestGonkCameraHardware::Init()
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      nsresult rv = mJSTestWrapper->InitCamera();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mTestHw->mDomListener = new TestGonkCameraHardwareListener(mTestHw->mTarget, mTestHw->mCameraThread);
      if (NS_WARN_IF(!mTestHw->mDomListener)) {
        return NS_ERROR_FAILURE;
      }

      rv = mJSTestWrapper->SetHandler(mTestHw->mDomListener);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }
  };

  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

int
TestGonkCameraHardware::AutoFocus()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->AutoFocus();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::CancelAutoFocus()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->CancelAutoFocus();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::StartFaceDetection()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StartFaceDetection();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::StopFaceDetection()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StopFaceDetection();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::TakePicture()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->TakePicture();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

void
TestGonkCameraHardware::CancelTakePicture()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->CancelTakePicture();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  NS_WARN_IF(NS_FAILED(rv));
}

int
TestGonkCameraHardware::StartPreview()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StartPreview();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

void
TestGonkCameraHardware::StopPreview()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StopPreview();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  NS_WARN_IF(NS_FAILED(rv));
}

class TestGonkCameraHardware::PushParametersDelegate : public ControlMessage
{
public:
  PushParametersDelegate(TestGonkCameraHardware* aTestHw, String8* aParams)
    : ControlMessage(aTestHw)
    , mParams(aParams)
  { }

protected:
  NS_IMETHOD
  RunImpl() override
  {
    if (NS_WARN_IF(!mParams)) {
      return NS_ERROR_INVALID_ARG;
    }

    DOM_CAMERA_LOGI("Push test parameters: %s\n", mParams->string());
    return mJSTestWrapper->PushParameters(NS_ConvertASCIItoUTF16(mParams->string()));
  }

  String8* mParams;
};

class TestGonkCameraHardware::PullParametersDelegate : public ControlMessage
{
public:
  PullParametersDelegate(TestGonkCameraHardware* aTestHw, nsString* aParams)
    : ControlMessage(aTestHw)
    , mParams(aParams)
  { }

protected:
  NS_IMETHOD
  RunImpl() override
  {
    if (NS_WARN_IF(!mParams)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsresult rv = mJSTestWrapper->PullParameters(*mParams);
    DOM_CAMERA_LOGI("Pull test parameters: %s\n",
      NS_LossyConvertUTF16toASCII(*mParams).get());
    return rv;
  }

  nsString* mParams;
};

int
TestGonkCameraHardware::PushParameters(const GonkCameraParameters& aParams)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  String8 s = aParams.Flatten();
  nsresult rv = WaitWhileRunningOnMainThread(new PushParametersDelegate(this, &s));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

nsresult
TestGonkCameraHardware::PullParameters(GonkCameraParameters& aParams)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsString as;
  nsresult rv = WaitWhileRunningOnMainThread(new PullParametersDelegate(this, &as));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  String8 s(NS_LossyConvertUTF16toASCII(as).get());
  aParams.Unflatten(s);
  return NS_OK;
}

#ifdef MOZ_WIDGET_GONK
int
TestGonkCameraHardware::PushParameters(const CameraParameters& aParams)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  String8 s = aParams.flatten();
  nsresult rv = WaitWhileRunningOnMainThread(new PushParametersDelegate(this, &s));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

void
TestGonkCameraHardware::PullParameters(CameraParameters& aParams)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsString as;
  nsresult rv = WaitWhileRunningOnMainThread(new PullParametersDelegate(this, &as));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    as.Truncate();
  }

  String8 s(NS_LossyConvertUTF16toASCII(as).get());
  aParams.unflatten(s);
}
#endif

int
TestGonkCameraHardware::StartRecording()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StartRecording();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::StopRecording()
{
  class Delegate : public ControlMessage
  {
  public:
    Delegate(TestGonkCameraHardware* aTestHw)
      : ControlMessage(aTestHw)
    { }

  protected:
    NS_IMETHOD
    RunImpl() override
    {
      return mJSTestWrapper->StopRecording();
    }
  };

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  nsresult rv = WaitWhileRunningOnMainThread(new Delegate(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

int
TestGonkCameraHardware::StoreMetaDataInBuffers(bool aEnabled)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  return OK;
}

