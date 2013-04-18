/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "DOMCameraPreview.h"
#include "CameraRecorderProfiles.h"
#include "CameraControlImpl.h"
#include "CameraCommon.h"
#include "nsGlobalWindow.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::idl;

CameraControlImpl::CameraControlImpl(uint32_t aCameraId, nsIThread* aCameraThread, uint64_t aWindowId)
  : mCameraId(aCameraId)
  , mCameraThread(aCameraThread)
  , mWindowId(aWindowId)
  , mFileFormat()
  , mMaxMeteringAreas(0)
  , mMaxFocusAreas(0)
  , mDOMPreview(nullptr)
  , mAutoFocusOnSuccessCb(nullptr)
  , mAutoFocusOnErrorCb(nullptr)
  , mTakePictureOnSuccessCb(nullptr)
  , mTakePictureOnErrorCb(nullptr)
  , mOnShutterCb(nullptr)
  , mOnClosedCb(nullptr)
  , mOnRecorderStateChangeCb(nullptr)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

CameraControlImpl::~CameraControlImpl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

// Helpers for string properties.
nsresult
CameraControlImpl::Set(uint32_t aKey, const nsAString& aValue)
{
  SetParameter(aKey, NS_ConvertUTF16toUTF8(aValue).get());
  return NS_OK;
}

nsresult
CameraControlImpl::Get(uint32_t aKey, nsAString& aValue)
{
  const char* value = GetParameterConstChar(aKey);
  if (!value) {
    return NS_ERROR_FAILURE;
  }

  aValue.AssignASCII(value);
  return NS_OK;
}

// Helpers for doubles.
nsresult
CameraControlImpl::Set(uint32_t aKey, double aValue)
{
  SetParameter(aKey, aValue);
  return NS_OK;
}

nsresult
CameraControlImpl::Get(uint32_t aKey, double* aValue)
{
  MOZ_ASSERT(aValue);
  *aValue = GetParameterDouble(aKey);
  return NS_OK;
}

// Helper for weighted regions.
nsresult
CameraControlImpl::Set(JSContext* aCx, uint32_t aKey, const JS::Value& aValue, uint32_t aLimit)
{
  if (aLimit == 0) {
    DOM_CAMERA_LOGI("%s:%d : aLimit = 0, nothing to do\n", __func__, __LINE__);
    return NS_OK;
  }

  if (!aValue.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length = 0;

  JSObject* regions = &aValue.toObject();
  if (!JS_GetArrayLength(aCx, regions, &length)) {
    return NS_ERROR_FAILURE;
  }

  DOM_CAMERA_LOGI("%s:%d : got %d regions (limited to %d)\n", __func__, __LINE__, length, aLimit);
  if (length > aLimit) {
    length = aLimit;
  }

  nsTArray<CameraRegion> regionArray;
  regionArray.SetCapacity(length);

  for (uint32_t i = 0; i < length; ++i) {
    JS::Value v;

    if (!JS_GetElement(aCx, regions, i, &v)) {
      return NS_ERROR_FAILURE;
    }

    CameraRegion* r = regionArray.AppendElement();
    /**
     * These are the default values.  We can remove these when the xpidl
     * dictionary parser gains the ability to grok default values.
     */
    r->top = -1000;
    r->left = -1000;
    r->bottom = 1000;
    r->right = 1000;
    r->weight = 1000;

    nsresult rv = r->Init(aCx, &v);
    NS_ENSURE_SUCCESS(rv, rv);

    DOM_CAMERA_LOGI("region %d: top=%d, left=%d, bottom=%d, right=%d, weight=%d\n",
      i,
      r->top,
      r->left,
      r->bottom,
      r->right,
      r->weight
    );
  }
  SetParameter(aKey, regionArray);
  return NS_OK;
}

nsresult
CameraControlImpl::Get(JSContext* aCx, uint32_t aKey, JS::Value* aValue)
{
  nsTArray<CameraRegion> regionArray;

  GetParameter(aKey, regionArray);

  JSObject* array = JS_NewArrayObject(aCx, 0, nullptr);
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t length = regionArray.Length();
  DOM_CAMERA_LOGI("%s:%d : got %d regions\n", __func__, __LINE__, length);

  for (uint32_t i = 0; i < length; ++i) {
    CameraRegion* r = &regionArray[i];
    JS::Value v;

    JSObject* o = JS_NewObject(aCx, nullptr, nullptr, nullptr);
    if (!o) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DOM_CAMERA_LOGI("top=%d\n", r->top);
    v = INT_TO_JSVAL(r->top);
    if (!JS_SetProperty(aCx, o, "top", &v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("left=%d\n", r->left);
    v = INT_TO_JSVAL(r->left);
    if (!JS_SetProperty(aCx, o, "left", &v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("bottom=%d\n", r->bottom);
    v = INT_TO_JSVAL(r->bottom);
    if (!JS_SetProperty(aCx, o, "bottom", &v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("right=%d\n", r->right);
    v = INT_TO_JSVAL(r->right);
    if (!JS_SetProperty(aCx, o, "right", &v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("weight=%d\n", r->weight);
    v = INT_TO_JSVAL(r->weight);
    if (!JS_SetProperty(aCx, o, "weight", &v)) {
      return NS_ERROR_FAILURE;
    }

    v = OBJECT_TO_JSVAL(o);
    if (!JS_SetElement(aCx, array, i, &v)) {
      return NS_ERROR_FAILURE;
    }
  }

  *aValue = JS::ObjectValue(*array);
  return NS_OK;
}

nsresult
CameraControlImpl::Set(nsICameraShutterCallback* aOnShutter)
{
  mOnShutterCb = new nsMainThreadPtrHolder<nsICameraShutterCallback>(aOnShutter);
  return NS_OK;
}

nsresult
CameraControlImpl::Get(nsICameraShutterCallback** aOnShutter)
{
  *aOnShutter = mOnShutterCb;
  return NS_OK;
}

nsresult
CameraControlImpl::Set(nsICameraClosedCallback* aOnClosed)
{
  mOnClosedCb = new nsMainThreadPtrHolder<nsICameraClosedCallback>(aOnClosed);
  return NS_OK;
}

nsresult
CameraControlImpl::Get(nsICameraClosedCallback** aOnClosed)
{
  *aOnClosed = mOnClosedCb;
  return NS_OK;
}

nsresult
CameraControlImpl::Set(nsICameraRecorderStateChange* aOnRecorderStateChange)
{
  mOnRecorderStateChangeCb = new nsMainThreadPtrHolder<nsICameraRecorderStateChange>(aOnRecorderStateChange);
  return NS_OK;
}

nsresult
CameraControlImpl::Get(nsICameraRecorderStateChange** aOnRecorderStateChange)
{
  *aOnRecorderStateChange = mOnRecorderStateChangeCb;
  return NS_OK;
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
  mAutoFocusOnSuccessCb = nullptr;
  mAutoFocusOnErrorCb = nullptr;
  mTakePictureOnSuccessCb = nullptr;
  mTakePictureOnErrorCb = nullptr;
  mOnShutterCb = nullptr;
  mOnClosedCb = nullptr;
  mOnRecorderStateChangeCb = nullptr;
}

void
CameraControlImpl::OnShutterInternal()
{
  DOM_CAMERA_LOGI("** SNAP **\n");
  if (mOnShutterCb.get()) {
    mOnShutterCb->HandleEvent();
  }
}

void
CameraControlImpl::OnShutter()
{
  nsCOMPtr<nsIRunnable> onShutter = NS_NewRunnableMethod(this, &CameraControlImpl::OnShutterInternal);
  nsresult rv = NS_DispatchToMainThread(onShutter);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("Failed to dispatch onShutter event to main thread (%d)\n", rv);
  }
}

class OnClosedTask : public nsRunnable
{
public:
  OnClosedTask(nsMainThreadPtrHandle<nsICameraClosedCallback> onClosed, uint64_t aWindowId)
    : mOnClosedCb(onClosed)
    , mWindowId(aWindowId)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  virtual ~OnClosedTask()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mOnClosedCb.get() && nsDOMCameraManager::IsWindowStillActive(mWindowId)) {
      mOnClosedCb->HandleEvent();
    }
    return NS_OK;
  }

protected:
  nsMainThreadPtrHandle<nsICameraClosedCallback> mOnClosedCb;
  uint64_t mWindowId;
};

void
CameraControlImpl::OnClosed()
{
  nsCOMPtr<nsIRunnable> onClosed = new OnClosedTask(mOnClosedCb, mWindowId);
  nsresult rv = NS_DispatchToMainThread(onClosed);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("Failed to dispatch onClosed event to main thread (%d)\n", rv);
  }
}

void
CameraControlImpl::OnRecorderStateChange(const nsString& aStateMsg, int32_t aStatus, int32_t aTrackNumber)
{
  DOM_CAMERA_LOGI("OnRecorderStateChange: '%s'\n", NS_ConvertUTF16toUTF8(aStateMsg).get());

  nsCOMPtr<nsIRunnable> onRecorderStateChange = new CameraRecorderStateChange(mOnRecorderStateChangeCb, aStateMsg, aStatus, aTrackNumber, mWindowId);
  nsresult rv = NS_DispatchToMainThread(onRecorderStateChange);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Failed to dispatch onRecorderStateChange event to main thread (%d)\n", rv);
  }
}

nsresult
CameraControlImpl::GetPreviewStream(CameraSize aSize, nsICameraPreviewStreamCallback* onSuccess, nsICameraErrorCallback* onError)
{
  nsCOMPtr<nsIRunnable> getPreviewStreamTask = new GetPreviewStreamTask(this, aSize, onSuccess, onError);
  return mCameraThread->Dispatch(getPreviewStreamTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::AutoFocus(nsICameraAutoFocusCallback* onSuccess, nsICameraErrorCallback* onError)
{
  MOZ_ASSERT(NS_IsMainThread());
  bool cancel = false;

  nsCOMPtr<nsICameraAutoFocusCallback> cb = mAutoFocusOnSuccessCb.get();
  if (cb) {
    /**
     * We already have a callback, so someone has already
     * called autoFocus() -- cancel it.
     */
    mAutoFocusOnSuccessCb = nullptr;
    mAutoFocusOnErrorCb = nullptr;
    cancel = true;
  }

  nsCOMPtr<nsIRunnable> autoFocusTask = new AutoFocusTask(this, cancel, onSuccess, onError);
  return mCameraThread->Dispatch(autoFocusTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::TakePicture(CameraSize aSize, int32_t aRotation, const nsAString& aFileFormat, CameraPosition aPosition, uint64_t aDateTime, nsICameraTakePictureCallback* onSuccess, nsICameraErrorCallback* onError)
{
  MOZ_ASSERT(NS_IsMainThread());
  bool cancel = false;

  nsCOMPtr<nsICameraTakePictureCallback> cb = mTakePictureOnSuccessCb.get();
  if (cb) {
    /**
     * We already have a callback, so someone has already
     * called takePicture() -- cancel it.
     */
    mTakePictureOnSuccessCb = nullptr;
    mTakePictureOnErrorCb = nullptr;
    cancel = true;
  }

  nsCOMPtr<nsIRunnable> takePictureTask = new TakePictureTask(this, cancel, aSize, aRotation, aFileFormat, aPosition, aDateTime, onSuccess, onError);
  return mCameraThread->Dispatch(takePictureTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StartRecording(CameraStartRecordingOptions* aOptions, nsIFile* aFolder, const nsAString& aFilename, nsICameraStartRecordingCallback* onSuccess, nsICameraErrorCallback* onError)
{
  nsCOMPtr<nsIFile> clone;
  aFolder->Clone(getter_AddRefs(clone));

  nsCOMPtr<nsIRunnable> startRecordingTask = new StartRecordingTask(this, *aOptions, clone, aFilename, onSuccess, onError, mWindowId);
  return mCameraThread->Dispatch(startRecordingTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StopRecording()
{
  nsCOMPtr<nsIRunnable> stopRecordingTask = new StopRecordingTask(this);
  return mCameraThread->Dispatch(stopRecordingTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::StartPreview(DOMCameraPreview* aDOMPreview)
{
  nsCOMPtr<nsIRunnable> startPreviewTask = new StartPreviewTask(this, aDOMPreview);
  return mCameraThread->Dispatch(startPreviewTask, NS_DISPATCH_NORMAL);
}

void
CameraControlImpl::StopPreview()
{
  nsCOMPtr<nsIRunnable> stopPreviewTask = new StopPreviewTask(this);
  mCameraThread->Dispatch(stopPreviewTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::GetPreviewStreamVideoMode(CameraRecorderOptions* aOptions, nsICameraPreviewStreamCallback* onSuccess, nsICameraErrorCallback* onError)
{
  nsCOMPtr<nsIRunnable> getPreviewStreamVideoModeTask = new GetPreviewStreamVideoModeTask(this, *aOptions, onSuccess, onError);
  return mCameraThread->Dispatch(getPreviewStreamVideoModeTask, NS_DISPATCH_NORMAL);
}

nsresult
CameraControlImpl::ReleaseHardware(nsICameraReleaseCallback* onSuccess, nsICameraErrorCallback* onError)
{
  nsCOMPtr<nsIRunnable> releaseHardwareTask = new ReleaseHardwareTask(this, onSuccess, onError);
  return mCameraThread->Dispatch(releaseHardwareTask, NS_DISPATCH_NORMAL);
}

bool
CameraControlImpl::ReceiveFrame(void* aBuffer, ImageFormat aFormat, FrameBuilder aBuilder)
{
  if (!mDOMPreview) {
    return false;
  }

  return mDOMPreview->ReceiveFrame(aBuffer, aFormat, aBuilder);
}

NS_IMETHODIMP
GetPreviewStreamResult::Run()
{
  /**
   * The camera preview stream object is DOM-facing, and as such
   * must be a cycle-collection participant created on the main
   * thread.
   */
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICameraPreviewStreamCallback> onSuccess = mOnSuccessCb.get();
  nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
  if (onSuccess && nsDOMCameraManager::IsWindowStillActive(mWindowId) && window) {
    nsCOMPtr<nsIDOMMediaStream> stream =
      new DOMCameraPreview(window, mCameraControl, mWidth, mHeight,
	                         mFramesPerSecond);
    onSuccess->HandleEvent(stream);
  }
  return NS_OK;
}
