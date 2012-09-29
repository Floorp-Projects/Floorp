/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfo.h"
#include "jsapi.h"
#include "nsThread.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIDOMDeviceStorage.h"
#include "DOMCameraManager.h"
#include "DOMCameraCapabilities.h"
#include "DOMCameraControl.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace dom;

DOMCI_DATA(CameraControl, nsICameraControl)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMCameraControl)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMCameraControl)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDOMCapabilities)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMCameraControl)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDOMCapabilities)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCameraControl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsICameraControl)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CameraControl)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCameraControl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCameraControl)

nsDOMCameraControl::~nsDOMCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/* readonly attribute nsICameraCapabilities capabilities; */
NS_IMETHODIMP
nsDOMCameraControl::GetCapabilities(nsICameraCapabilities** aCapabilities)
{
  if (!mDOMCapabilities) {
    mDOMCapabilities = new DOMCameraCapabilities(mCameraControl);
  }

  nsCOMPtr<nsICameraCapabilities> capabilities = mDOMCapabilities;
  capabilities.forget(aCapabilities);
  return NS_OK;
}

/* attribute DOMString effect; */
NS_IMETHODIMP
nsDOMCameraControl::GetEffect(nsAString& aEffect)
{
  return mCameraControl->Get(CAMERA_PARAM_EFFECT, aEffect);
}
NS_IMETHODIMP
nsDOMCameraControl::SetEffect(const nsAString& aEffect)
{
  return mCameraControl->Set(CAMERA_PARAM_EFFECT, aEffect);
}

/* attribute DOMString whiteBalanceMode; */
NS_IMETHODIMP
nsDOMCameraControl::GetWhiteBalanceMode(nsAString& aWhiteBalanceMode)
{
  return mCameraControl->Get(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}
NS_IMETHODIMP
nsDOMCameraControl::SetWhiteBalanceMode(const nsAString& aWhiteBalanceMode)
{
  return mCameraControl->Set(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}

/* attribute DOMString sceneMode; */
NS_IMETHODIMP
nsDOMCameraControl::GetSceneMode(nsAString& aSceneMode)
{
  return mCameraControl->Get(CAMERA_PARAM_SCENEMODE, aSceneMode);
}
NS_IMETHODIMP
nsDOMCameraControl::SetSceneMode(const nsAString& aSceneMode)
{
  return mCameraControl->Set(CAMERA_PARAM_SCENEMODE, aSceneMode);
}

/* attribute DOMString flashMode; */
NS_IMETHODIMP
nsDOMCameraControl::GetFlashMode(nsAString& aFlashMode)
{
  return mCameraControl->Get(CAMERA_PARAM_FLASHMODE, aFlashMode);
}
NS_IMETHODIMP
nsDOMCameraControl::SetFlashMode(const nsAString& aFlashMode)
{
  return mCameraControl->Set(CAMERA_PARAM_FLASHMODE, aFlashMode);
}

/* attribute DOMString focusMode; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocusMode(nsAString& aFocusMode)
{
  return mCameraControl->Get(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}
NS_IMETHODIMP
nsDOMCameraControl::SetFocusMode(const nsAString& aFocusMode)
{
  return mCameraControl->Set(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}

/* attribute double zoom; */
NS_IMETHODIMP
nsDOMCameraControl::GetZoom(double* aZoom)
{
  return mCameraControl->Get(CAMERA_PARAM_ZOOM, aZoom);
}
NS_IMETHODIMP
nsDOMCameraControl::SetZoom(double aZoom)
{
  return mCameraControl->Set(CAMERA_PARAM_ZOOM, aZoom);
}

/* attribute jsval meteringAreas; */
NS_IMETHODIMP
nsDOMCameraControl::GetMeteringAreas(JSContext* cx, JS::Value* aMeteringAreas)
{
  return mCameraControl->Get(cx, CAMERA_PARAM_METERINGAREAS, aMeteringAreas);
}
NS_IMETHODIMP
nsDOMCameraControl::SetMeteringAreas(JSContext* cx, const JS::Value& aMeteringAreas)
{
  return mCameraControl->SetMeteringAreas(cx, aMeteringAreas);
}

/* attribute jsval focusAreas; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocusAreas(JSContext* cx, JS::Value* aFocusAreas)
{
  return mCameraControl->Get(cx, CAMERA_PARAM_FOCUSAREAS, aFocusAreas);
}
NS_IMETHODIMP
nsDOMCameraControl::SetFocusAreas(JSContext* cx, const JS::Value& aFocusAreas)
{
  return mCameraControl->SetFocusAreas(cx, aFocusAreas);
}

/* readonly attribute double focalLength; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocalLength(double* aFocalLength)
{
  return mCameraControl->Get(CAMERA_PARAM_FOCALLENGTH, aFocalLength);
}

/* readonly attribute double focusDistanceNear; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocusDistanceNear(double* aFocusDistanceNear)
{
  return mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCENEAR, aFocusDistanceNear);
}

/* readonly attribute double focusDistanceOptimum; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocusDistanceOptimum(double* aFocusDistanceOptimum)
{
  return mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEOPTIMUM, aFocusDistanceOptimum);
}

/* readonly attribute double focusDistanceFar; */
NS_IMETHODIMP
nsDOMCameraControl::GetFocusDistanceFar(double* aFocusDistanceFar)
{
  return mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEFAR, aFocusDistanceFar);
}

/* void setExposureCompensation (const JS::Value& aCompensation, JSContext* cx); */
NS_IMETHODIMP
nsDOMCameraControl::SetExposureCompensation(const JS::Value& aCompensation, JSContext* cx)
{
  if (aCompensation.isNullOrUndefined()) {
    // use NaN to switch the camera back into auto mode
    return mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, NAN);
  }

  double compensation;
  if (!JS_ValueToNumber(cx, aCompensation, &compensation)) {
    return NS_ERROR_INVALID_ARG;
  }

  return mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, compensation);
}

/* readonly attribute double exposureCompensation; */
NS_IMETHODIMP
nsDOMCameraControl::GetExposureCompensation(double* aExposureCompensation)
{
  return mCameraControl->Get(CAMERA_PARAM_EXPOSURECOMPENSATION, aExposureCompensation);
}

/* attribute nsICameraShutterCallback onShutter; */
NS_IMETHODIMP
nsDOMCameraControl::GetOnShutter(nsICameraShutterCallback** aOnShutter)
{
  // TODO: see bug 779138.
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsDOMCameraControl::SetOnShutter(nsICameraShutterCallback* aOnShutter)
{
  // TODO: see bug 779138.
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [implicit_jscontext] void startRecording (in nsIDOMDeviceStorage storageArea, in DOMString filename, in nsICameraStartRecordingCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraControl::StartRecording(nsIDOMDeviceStorage* storageArea, const nsAString& filename, nsICameraStartRecordingCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not get the Observer service for CameraControl::StartRecording.");
    return NS_ERROR_FAILURE;
  }

  obs->NotifyObservers(nullptr,
                       "recording-device-events",
                       NS_LITERAL_STRING("starting").get());

  return mCameraControl->StartRecording(storageArea, filename, onSuccess, onError);
}

/* void stopRecording (); */
NS_IMETHODIMP
nsDOMCameraControl::StopRecording()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not get the Observer service for CameraControl::StopRecording.");
    return NS_ERROR_FAILURE;
  }

  obs->NotifyObservers(nullptr,
                       "recording-device-events",
                       NS_LITERAL_STRING("shutdown").get());

  return mCameraControl->StopRecording();
}

/* [implicit_jscontext] void getPreviewStream (in jsval aOptions, in nsICameraPreviewStreamCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraControl::GetPreviewStream(const JS::Value& aOptions, nsICameraPreviewStreamCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  CameraSize size;
  nsresult rv = size.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  return mCameraControl->GetPreviewStream(size, onSuccess, onError);
}

/* void resumePreview(); */
NS_IMETHODIMP
nsDOMCameraControl::ResumePreview()
{
  return mCameraControl->StartPreview(nullptr);
}

/* void autoFocus (in nsICameraAutoFocusCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraControl::AutoFocus(nsICameraAutoFocusCallback* onSuccess, nsICameraErrorCallback* onError)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);
  return mCameraControl->AutoFocus(onSuccess, onError);
}

/* void takePicture (in jsval aOptions, in nsICameraTakePictureCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraControl::TakePicture(const JS::Value& aOptions, nsICameraTakePictureCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  CameraPictureOptions  options;
  CameraSize            size;
  CameraPosition        pos;

  nsresult rv = options.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = size.Init(cx, &options.pictureSize);
  NS_ENSURE_SUCCESS(rv, rv);

  /**
   * Default values, until the dictionary parser can handle them.
   * NaN indicates no value provided.
   */
  pos.latitude = NAN;
  pos.longitude = NAN;
  pos.altitude = NAN;
  pos.timestamp = NAN;
  rv = pos.Init(cx, &options.position);
  NS_ENSURE_SUCCESS(rv, rv);

  return mCameraControl->TakePicture(size, options.rotation, options.fileFormat, pos, onSuccess, onError);
}

/* [implicit_jscontext] void GetPreviewStreamVideoMode (in jsval aOptions, in nsICameraPreviewStreamCallback onSuccess, [optional] in nsICameraErrorCallback onError); */
NS_IMETHODIMP
nsDOMCameraControl::GetPreviewStreamVideoMode(const JS::Value& aOptions, nsICameraPreviewStreamCallback* onSuccess, nsICameraErrorCallback* onError, JSContext* cx)
{
  NS_ENSURE_TRUE(onSuccess, NS_ERROR_INVALID_ARG);

  CameraRecordingOptions options;
  nsresult rv = options.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  return mCameraControl->GetPreviewStreamVideoMode(&options, onSuccess, onError);
}

class GetCameraResult : public nsRunnable
{
public:
  GetCameraResult(nsDOMCameraControl* aDOMCameraControl, nsresult aResult, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError)
    : mDOMCameraControl(aDOMCameraControl)
    , mResult(aResult)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    DOM_CAMERA_LOGT("%s : this=%p -- BEFORE CALLBACK\n", __func__, this);
    if (NS_FAILED(mResult)) {
      if (mOnErrorCb) {
        mOnErrorCb->HandleEvent(NS_LITERAL_STRING("FAILURE"));
      }
    } else {
      if (mOnSuccessCb) {
        mOnSuccessCb->HandleEvent(mDOMCameraControl);
      }
    }
    DOM_CAMERA_LOGT("%s : this=%p -- AFTER CALLBACK\n", __func__, this);

    /**
     * Finally, release the extra reference to the DOM-facing camera control.
     * See the nsDOMCameraControl constructor for the corresponding call to
     * NS_ADDREF_THIS().
     */
    NS_RELEASE(mDOMCameraControl);
    return NS_OK;
  }

protected:
  /**
   * 'mDOMCameraControl' is a raw pointer to a previously ADDREF()ed object,
   * which is released in Run().
   */
  nsDOMCameraControl* mDOMCameraControl;
  nsresult mResult;
  nsCOMPtr<nsICameraGetCameraCallback> mOnSuccessCb;
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
};

nsresult
nsDOMCameraControl::Result(nsresult aResult, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError)
{
  nsCOMPtr<GetCameraResult> getCameraResult = new GetCameraResult(this, aResult, onSuccess, onError);
  return NS_DispatchToMainThread(getCameraResult);
}
