/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraControl.h"
#include "base/basictypes.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfo.h"
#include "nsHashPropertyBag.h"
#include "nsThread.h"
#include "DeviceStorage.h"
#include "DeviceStorageFileDescriptor.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIAppsService.h"
#include "nsIObserverService.h"
#include "nsIDOMDeviceStorage.h"
#include "nsIDOMEventListener.h"
#include "nsIScriptSecurityManager.h"
#include "Navigator.h"
#include "nsXULAppAPI.h"
#include "DOMCameraManager.h"
#include "DOMCameraCapabilities.h"
#include "CameraCommon.h"
#include "nsGlobalWindow.h"
#include "CameraPreviewMediaStream.h"
#include "mozilla/dom/CameraControlBinding.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/CameraCapabilitiesBinding.h"
#include "DOMCameraDetectedFace.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMCameraControl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

NS_IMPL_ADDREF_INHERITED(nsDOMCameraControl, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(nsDOMCameraControl, DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_INHERITED_20(nsDOMCameraControl, DOMMediaStream,
                                      mCapabilities,
                                      mWindow,
                                      mGetCameraOnSuccessCb,
                                      mGetCameraOnErrorCb,
                                      mAutoFocusOnSuccessCb,
                                      mAutoFocusOnErrorCb,
                                      mTakePictureOnSuccessCb,
                                      mTakePictureOnErrorCb,
                                      mStartRecordingOnSuccessCb,
                                      mStartRecordingOnErrorCb,
                                      mReleaseOnSuccessCb,
                                      mReleaseOnErrorCb,
                                      mSetConfigurationOnSuccessCb,
                                      mSetConfigurationOnErrorCb,
                                      mOnShutterCb,
                                      mOnClosedCb,
                                      mOnRecorderStateChangeCb,
                                      mOnPreviewStateChangeCb,
                                      mOnAutoFocusMovingCb,
                                      mOnFacesDetectedCb)

/* static */
bool
nsDOMCameraControl::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

class mozilla::StartRecordingHelper : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  StartRecordingHelper(nsDOMCameraControl* aDOMCameraControl)
    : mDOMCameraControl(aDOMCameraControl)
  {
    MOZ_COUNT_CTOR(StartRecordingHelper);
  }

protected:
  virtual ~StartRecordingHelper()
  {
    MOZ_COUNT_DTOR(StartRecordingHelper);
  }

protected:
  nsRefPtr<nsDOMCameraControl> mDOMCameraControl;
};

NS_IMETHODIMP
StartRecordingHelper::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString eventType;
  aEvent->GetType(eventType);

  mDOMCameraControl->OnCreatedFileDescriptor(eventType.EqualsLiteral("success"));
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(mozilla::StartRecordingHelper, nsIDOMEventListener)

nsDOMCameraControl::DOMCameraConfiguration::DOMCameraConfiguration()
  : CameraConfiguration()
  , mMaxFocusAreas(0)
  , mMaxMeteringAreas(0)
{
  MOZ_COUNT_CTOR(nsDOMCameraControl::DOMCameraConfiguration);
}

nsDOMCameraControl::DOMCameraConfiguration::DOMCameraConfiguration(const CameraConfiguration& aConfiguration)
  : CameraConfiguration(aConfiguration)
  , mMaxFocusAreas(0)
  , mMaxMeteringAreas(0)
{
  MOZ_COUNT_CTOR(nsDOMCameraControl::DOMCameraConfiguration);
}

nsDOMCameraControl::DOMCameraConfiguration::~DOMCameraConfiguration()
{
  MOZ_COUNT_DTOR(nsDOMCameraControl::DOMCameraConfiguration);
}

nsDOMCameraControl::nsDOMCameraControl(uint32_t aCameraId,
                                       const CameraConfiguration& aInitialConfig,
                                       GetCameraCallback* aOnSuccess,
                                       CameraErrorCallback* aOnError,
                                       nsPIDOMWindow* aWindow)
  : DOMMediaStream()
  , mCameraControl(nullptr)
  , mAudioChannelAgent(nullptr)
  , mGetCameraOnSuccessCb(aOnSuccess)
  , mGetCameraOnErrorCb(aOnError)
  , mAutoFocusOnSuccessCb(nullptr)
  , mAutoFocusOnErrorCb(nullptr)
  , mTakePictureOnSuccessCb(nullptr)
  , mTakePictureOnErrorCb(nullptr)
  , mStartRecordingOnSuccessCb(nullptr)
  , mStartRecordingOnErrorCb(nullptr)
  , mReleaseOnSuccessCb(nullptr)
  , mReleaseOnErrorCb(nullptr)
  , mSetConfigurationOnSuccessCb(nullptr)
  , mSetConfigurationOnErrorCb(nullptr)
  , mOnShutterCb(nullptr)
  , mOnClosedCb(nullptr)
  , mOnRecorderStateChangeCb(nullptr)
  , mOnPreviewStateChangeCb(nullptr)
  , mOnAutoFocusMovingCb(nullptr)
  , mOnFacesDetectedCb(nullptr)
  , mWindow(aWindow)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mInput = new CameraPreviewMediaStream(this);

  SetIsDOMBinding();

  nsRefPtr<DOMCameraConfiguration> initialConfig =
    new DOMCameraConfiguration(aInitialConfig);

  // Create and initialize the underlying camera.
  ICameraControl::Configuration config;

  switch (aInitialConfig.mMode) {
    case CameraMode::Picture:
      config.mMode = ICameraControl::kPictureMode;
      break;

    case CameraMode::Video:
      config.mMode = ICameraControl::kVideoMode;
      break;

    default:
      MOZ_ASSUME_UNREACHABLE("Unanticipated camera mode!");
  }

  config.mPreviewSize.width = aInitialConfig.mPreviewSize.mWidth;
  config.mPreviewSize.height = aInitialConfig.mPreviewSize.mHeight;
  config.mRecorderProfile = aInitialConfig.mRecorderProfile;

  mCameraControl = ICameraControl::Create(aCameraId);
  mCurrentConfiguration = initialConfig.forget();

  // Attach our DOM-facing media stream to our viewfinder stream.
  mStream = mInput;
  MOZ_ASSERT(mWindow, "Shouldn't be created with a null window!");
  if (mWindow->GetExtantDoc()) {
    CombineWithPrincipal(mWindow->GetExtantDoc()->NodePrincipal());
  }

  // Register a listener for camera events.
  mListener = new DOMCameraControlListener(this, mInput);
  mCameraControl->AddListener(mListener);

  // Start the camera...
  nsresult rv = mCameraControl->Start(&config);
  if (NS_FAILED(rv)) {
    mListener->OnError(DOMCameraControlListener::kInStartCamera,
                       DOMCameraControlListener::kErrorApiFailed);
  }
}

nsDOMCameraControl::~nsDOMCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

JSObject*
nsDOMCameraControl::WrapObject(JSContext* aCx)
{
  return CameraControlBinding::Wrap(aCx, this);
}

bool
nsDOMCameraControl::IsWindowStillActive()
{
  return nsDOMCameraManager::IsWindowStillActive(mWindow->WindowID());
}

// JS-to-native helpers
// Setter for weighted regions: { top, bottom, left, right, weight }
nsresult
nsDOMCameraControl::Set(JSContext* aCx, uint32_t aKey, const JS::Value& aValue, uint32_t aLimit)
{
  if (aLimit == 0) {
    DOM_CAMERA_LOGI("%s:%d : aLimit = 0, nothing to do\n", __func__, __LINE__);
    return NS_OK;
  }

  if (!aValue.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length = 0;

  JS::Rooted<JSObject*> regions(aCx, &aValue.toObject());
  if (!JS_GetArrayLength(aCx, regions, &length)) {
    return NS_ERROR_FAILURE;
  }

  DOM_CAMERA_LOGI("%s:%d : got %d regions (limited to %d)\n", __func__, __LINE__, length, aLimit);
  if (length > aLimit) {
    length = aLimit;
  }

  nsTArray<ICameraControl::Region> regionArray;
  regionArray.SetCapacity(length);

  for (uint32_t i = 0; i < length; ++i) {
    JS::Rooted<JS::Value> v(aCx);

    if (!JS_GetElement(aCx, regions, i, &v)) {
      return NS_ERROR_FAILURE;
    }

    CameraRegion region;
    if (!region.Init(aCx, v)) {
      return NS_ERROR_FAILURE;
    }

    ICameraControl::Region* r = regionArray.AppendElement();
    r->top = region.mTop;
    r->left = region.mLeft;
    r->bottom = region.mBottom;
    r->right = region.mRight;
    r->weight = region.mWeight;

    DOM_CAMERA_LOGI("region %d: top=%d, left=%d, bottom=%d, right=%d, weight=%u\n",
      i,
      r->top,
      r->left,
      r->bottom,
      r->right,
      r->weight
    );
  }
  return mCameraControl->Set(aKey, regionArray);
}

// Getter for weighted regions: { top, bottom, left, right, weight }
nsresult
nsDOMCameraControl::Get(JSContext* aCx, uint32_t aKey, JS::Value* aValue)
{
  nsTArray<ICameraControl::Region> regionArray;

  nsresult rv = mCameraControl->Get(aKey, regionArray);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t length = regionArray.Length();
  DOM_CAMERA_LOGI("%s:%d : got %d regions\n", __func__, __LINE__, length);

  for (uint32_t i = 0; i < length; ++i) {
    ICameraControl::Region* r = &regionArray[i];
    JS::Rooted<JS::Value> v(aCx);

    JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!o) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    DOM_CAMERA_LOGI("top=%d\n", r->top);
    v = INT_TO_JSVAL(r->top);
    if (!JS_SetProperty(aCx, o, "top", v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("left=%d\n", r->left);
    v = INT_TO_JSVAL(r->left);
    if (!JS_SetProperty(aCx, o, "left", v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("bottom=%d\n", r->bottom);
    v = INT_TO_JSVAL(r->bottom);
    if (!JS_SetProperty(aCx, o, "bottom", v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("right=%d\n", r->right);
    v = INT_TO_JSVAL(r->right);
    if (!JS_SetProperty(aCx, o, "right", v)) {
      return NS_ERROR_FAILURE;
    }
    DOM_CAMERA_LOGI("weight=%d\n", r->weight);
    v = INT_TO_JSVAL(r->weight);
    if (!JS_SetProperty(aCx, o, "weight", v)) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_SetElement(aCx, array, i, o)) {
      return NS_ERROR_FAILURE;
    }
  }

  *aValue = JS::ObjectValue(*array);
  return NS_OK;
}

void
nsDOMCameraControl::GetEffect(nsString& aEffect, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_EFFECT, aEffect);
}
void
nsDOMCameraControl::SetEffect(const nsAString& aEffect, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_EFFECT, aEffect);
}

void
nsDOMCameraControl::GetWhiteBalanceMode(nsString& aWhiteBalanceMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}
void
nsDOMCameraControl::SetWhiteBalanceMode(const nsAString& aWhiteBalanceMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}

void
nsDOMCameraControl::GetSceneMode(nsString& aSceneMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_SCENEMODE, aSceneMode);
}
void
nsDOMCameraControl::SetSceneMode(const nsAString& aSceneMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_SCENEMODE, aSceneMode);
}

void
nsDOMCameraControl::GetFlashMode(nsString& aFlashMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_FLASHMODE, aFlashMode);
}
void
nsDOMCameraControl::SetFlashMode(const nsAString& aFlashMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_FLASHMODE, aFlashMode);
}

void
nsDOMCameraControl::GetFocusMode(nsString& aFocusMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}
void
nsDOMCameraControl::SetFocusMode(const nsAString& aFocusMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}

void
nsDOMCameraControl::GetIsoMode(nsString& aIsoMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Get(CAMERA_PARAM_ISOMODE, aIsoMode);
}
void
nsDOMCameraControl::SetIsoMode(const nsAString& aIsoMode, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_ISOMODE, aIsoMode);
}

double
nsDOMCameraControl::GetZoom(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double zoom;
  aRv = mCameraControl->Get(CAMERA_PARAM_ZOOM, zoom);
  return zoom;
}

void
nsDOMCameraControl::SetZoom(double aZoom, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_ZOOM, aZoom);
}

/* attribute jsval meteringAreas; */
JS::Value
nsDOMCameraControl::GetMeteringAreas(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> areas(cx);
  aRv = Get(cx, CAMERA_PARAM_METERINGAREAS, areas.address());
  return areas;
}

void
nsDOMCameraControl::SetMeteringAreas(JSContext* cx, JS::Handle<JS::Value> aMeteringAreas, ErrorResult& aRv)
{
  aRv = Set(cx, CAMERA_PARAM_METERINGAREAS, aMeteringAreas,
            mCurrentConfiguration->mMaxMeteringAreas);
}

JS::Value
nsDOMCameraControl::GetFocusAreas(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(cx);
  aRv = Get(cx, CAMERA_PARAM_FOCUSAREAS, value.address());
  return value;
}
void
nsDOMCameraControl::SetFocusAreas(JSContext* cx, JS::Handle<JS::Value> aFocusAreas, ErrorResult& aRv)
{
  aRv = Set(cx, CAMERA_PARAM_FOCUSAREAS, aFocusAreas,
            mCurrentConfiguration->mMaxFocusAreas);
}

static nsresult
GetSize(JSContext* aCx, JS::Value* aValue, const ICameraControl::Size& aSize)
{
  JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!o) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JS::Value> v(aCx);

  v = INT_TO_JSVAL(aSize.width);
  if (!JS_SetProperty(aCx, o, "width", v)) {
    return NS_ERROR_FAILURE;
  }
  v = INT_TO_JSVAL(aSize.height);
  if (!JS_SetProperty(aCx, o, "height", v)) {
    return NS_ERROR_FAILURE;
  }

  *aValue = JS::ObjectValue(*o);
  return NS_OK;
}

/* attribute any pictureSize */
JS::Value
nsDOMCameraControl::GetPictureSize(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(cx);

  ICameraControl::Size size;
  aRv = mCameraControl->Get(CAMERA_PARAM_PICTURE_SIZE, size);
  if (aRv.Failed()) {
    return value;
  }

  aRv = GetSize(cx, value.address(), size);
  return value;
}
void
nsDOMCameraControl::SetPictureSize(JSContext* aCx, JS::Handle<JS::Value> aSize, ErrorResult& aRv)
{
  CameraSize size;
  if (!size.Init(aCx, aSize)) {
    aRv = NS_ERROR_FAILURE;
    return;
  }

  ICameraControl::Size s = { size.mWidth, size.mHeight };
  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURE_SIZE, s);
}

/* attribute any thumbnailSize */
JS::Value
nsDOMCameraControl::GetThumbnailSize(JSContext* aCx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(aCx);

  ICameraControl::Size size;
  aRv = mCameraControl->Get(CAMERA_PARAM_THUMBNAILSIZE, size);
  if (aRv.Failed()) {
    return value;
  }

  aRv = GetSize(aCx, value.address(), size);
  return value;
}
void
nsDOMCameraControl::SetThumbnailSize(JSContext* aCx, JS::Handle<JS::Value> aSize, ErrorResult& aRv)
{
  CameraSize size;
  if (!size.Init(aCx, aSize)) {
    aRv = NS_ERROR_FAILURE;
    return;
  }

  ICameraControl::Size s = { size.mWidth, size.mHeight };
  aRv = mCameraControl->Set(CAMERA_PARAM_THUMBNAILSIZE, s);
}

double
nsDOMCameraControl::GetFocalLength(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double focalLength;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCALLENGTH, focalLength);
  return focalLength;
}

double
nsDOMCameraControl::GetFocusDistanceNear(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCENEAR, distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceOptimum(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEOPTIMUM, distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceFar(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEFAR, distance);
  return distance;
}

void
nsDOMCameraControl::SetExposureCompensation(const Optional<double>& aCompensation, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  if (!aCompensation.WasPassed()) {
    // use NaN to switch the camera back into auto mode
    aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, NAN);
    return;
  }

  aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, aCompensation.Value());
}

double
nsDOMCameraControl::GetExposureCompensation(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double compensation;
  aRv = mCameraControl->Get(CAMERA_PARAM_EXPOSURECOMPENSATION, compensation);
  return compensation;
}

int32_t
nsDOMCameraControl::SensorAngle()
{
  MOZ_ASSERT(mCameraControl);

  int32_t angle = 0;
  mCameraControl->Get(CAMERA_PARAM_SENSORANGLE, angle);
  return angle;
}

// Callback attributes

CameraShutterCallback*
nsDOMCameraControl::GetOnShutter()
{
  return mOnShutterCb;
}
void
nsDOMCameraControl::SetOnShutter(CameraShutterCallback* aCb)
{
  mOnShutterCb = aCb;
}

CameraClosedCallback*
nsDOMCameraControl::GetOnClosed()
{
  return mOnClosedCb;
}
void
nsDOMCameraControl::SetOnClosed(CameraClosedCallback* aCb)
{
  mOnClosedCb = aCb;
}

CameraRecorderStateChange*
nsDOMCameraControl::GetOnRecorderStateChange()
{
  return mOnRecorderStateChangeCb;
}
void
nsDOMCameraControl::SetOnRecorderStateChange(CameraRecorderStateChange* aCb)
{
  mOnRecorderStateChangeCb = aCb;
}

CameraPreviewStateChange*
nsDOMCameraControl::GetOnPreviewStateChange()
{
  return mOnPreviewStateChangeCb;
}
void
nsDOMCameraControl::SetOnPreviewStateChange(CameraPreviewStateChange* aCb)
{
  mOnPreviewStateChangeCb = aCb;
}

CameraAutoFocusMovingCallback*
nsDOMCameraControl::GetOnAutoFocusMoving()
{
  return mOnAutoFocusMovingCb;
}
void
nsDOMCameraControl::SetOnAutoFocusMoving(CameraAutoFocusMovingCallback* aCb)
{
  mOnAutoFocusMovingCb = aCb;
}

CameraFaceDetectionCallback*
nsDOMCameraControl::GetOnFacesDetected()
{
  return mOnFacesDetectedCb;
}
void
nsDOMCameraControl::SetOnFacesDetected(CameraFaceDetectionCallback* aCb)
{
  mOnFacesDetectedCb = aCb;
}

already_AddRefed<dom::CameraCapabilities>
nsDOMCameraControl::Capabilities()
{
  nsRefPtr<CameraCapabilities> caps = mCapabilities;

  if (!caps) {
    caps = new CameraCapabilities(mWindow);
    nsresult rv = caps->Populate(mCameraControl);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGW("Failed to populate camera capabilities (%d)\n", rv);
      return nullptr;
    }
    mCapabilities = caps;
  }

  return caps.forget();
}

// Methods.
void
nsDOMCameraControl::StartRecording(const CameraStartRecordingOptions& aOptions,
                                   nsDOMDeviceStorage& aStorageArea,
                                   const nsAString& aFilename,
                                   CameraStartRecordingCallback& aOnSuccess,
                                   const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  NotifyRecordingStatusChange(NS_LITERAL_STRING("starting"));

#ifdef MOZ_B2G
  if (!mAudioChannelAgent) {
    mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
    if (mAudioChannelAgent) {
      // Camera app will stop recording when it falls to the background, so no callback is necessary.
      mAudioChannelAgent->Init(mWindow, (int32_t)AudioChannel::Content, nullptr);
      // Video recording doesn't output any sound, so it's not necessary to check canPlay.
      int32_t canPlay;
      mAudioChannelAgent->StartPlaying(&canPlay);
    }
  }
#endif

  nsCOMPtr<nsIDOMDOMRequest> request;
  mDSFileDescriptor = new DeviceStorageFileDescriptor();
  aRv = aStorageArea.CreateFileDescriptor(aFilename, mDSFileDescriptor.get(),
                                         getter_AddRefs(request));
  if (aRv.Failed()) {
    return;
  }

  mOptions = aOptions;
  mStartRecordingOnSuccessCb = &aOnSuccess;
  mStartRecordingOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mStartRecordingOnErrorCb = &aOnError.Value();
  }

  nsCOMPtr<nsIDOMEventListener> listener = new StartRecordingHelper(this);
  request->AddEventListener(NS_LITERAL_STRING("success"), listener, false);
  request->AddEventListener(NS_LITERAL_STRING("error"), listener, false);
}

void
nsDOMCameraControl::OnCreatedFileDescriptor(bool aSucceeded)
{
  if (aSucceeded && mDSFileDescriptor->mFileDescriptor.IsValid()) {
    ICameraControl::StartRecordingOptions o;

    o.rotation = mOptions.mRotation;
    o.maxFileSizeBytes = mOptions.mMaxFileSizeBytes;
    o.maxVideoLengthMs = mOptions.mMaxVideoLengthMs;
    o.autoEnableLowLightTorch = mOptions.mAutoEnableLowLightTorch;
    nsresult rv = mCameraControl->StartRecording(mDSFileDescriptor.get(), &o);
    if (NS_SUCCEEDED(rv)) {
      return;
    }
  }
  OnError(CameraControlListener::kInStartRecording, NS_LITERAL_STRING("FAILURE"));

  if (mDSFileDescriptor->mFileDescriptor.IsValid()) {
    // An error occured. We need to manually close the file associated with the
    // FileDescriptor, and we shouldn't do this on the main thread, so we
    // use a little helper.
    nsRefPtr<CloseFileRunnable> closer =
      new CloseFileRunnable(mDSFileDescriptor->mFileDescriptor);
    closer->Dispatch();
  }
}

void
nsDOMCameraControl::StopRecording(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

#ifdef MOZ_B2G
  if (mAudioChannelAgent) {
    mAudioChannelAgent->StopPlaying();
    mAudioChannelAgent = nullptr;
  }
#endif

  aRv = mCameraControl->StopRecording();
}

void
nsDOMCameraControl::ResumePreview(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->StartPreview();
}

void
nsDOMCameraControl::SetConfiguration(const CameraConfiguration& aConfiguration,
                                     const Optional<OwningNonNull<CameraSetConfigurationCallback> >& aOnSuccess,
                                     const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<CameraTakePictureCallback> cb = mTakePictureOnSuccessCb;
  if (cb) {
    // We're busy taking a picture, can't change modes right now.
    if (aOnError.WasPassed()) {
      ErrorResult ignored;
      aOnError.Value().Call(NS_LITERAL_STRING("Busy"), ignored);
    }
    aRv = NS_ERROR_FAILURE;
    return;
  }

  ICameraControl::Configuration config;
  config.mRecorderProfile = aConfiguration.mRecorderProfile;
  config.mPreviewSize.width = aConfiguration.mPreviewSize.mWidth;
  config.mPreviewSize.height = aConfiguration.mPreviewSize.mHeight;
  config.mMode = ICameraControl::kPictureMode;
  if (aConfiguration.mMode == CameraMode::Video) {
    config.mMode = ICameraControl::kVideoMode;
  }

  mSetConfigurationOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mSetConfigurationOnSuccessCb = &aOnSuccess.Value();
  }
  mSetConfigurationOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mSetConfigurationOnErrorCb = &aOnError.Value();
  }

  aRv = mCameraControl->SetConfiguration(config);
}

class ImmediateErrorCallback : public nsRunnable
{
public:
  ImmediateErrorCallback(CameraErrorCallback* aCallback, const nsAString& aMessage)
    : mCallback(aCallback)
    , mMessage(aMessage)
  { }
  
  NS_IMETHODIMP
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    ErrorResult ignored;
    mCallback->Call(mMessage, ignored);
    return NS_OK;
  }

protected:
  nsRefPtr<CameraErrorCallback> mCallback;
  nsString mMessage;
};


void
nsDOMCameraControl::AutoFocus(CameraAutoFocusCallback& aOnSuccess,
                              const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                              ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<CameraAutoFocusCallback> cb = mAutoFocusOnSuccessCb;
  if (cb) {
    if (aOnError.WasPassed()) {
      // There is already a call to AutoFocus() in progress, abort this new one
      // and invoke the error callback (if one was passed in).
      NS_DispatchToMainThread(new ImmediateErrorCallback(&aOnError.Value(),
                              NS_LITERAL_STRING("AutoFocusAlreadyInProgress")));
    }
    aRv = NS_ERROR_FAILURE;
    return;
  }

  mAutoFocusOnSuccessCb = &aOnSuccess;
  mAutoFocusOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mAutoFocusOnErrorCb = &aOnError.Value();
  }

  aRv = mCameraControl->AutoFocus();
}

void
nsDOMCameraControl::StartFaceDetection(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->StartFaceDetection();
}

void
nsDOMCameraControl::StopFaceDetection(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->StopFaceDetection();
}

void
nsDOMCameraControl::TakePicture(const CameraPictureOptions& aOptions,
                                CameraTakePictureCallback& aOnSuccess,
                                const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<CameraTakePictureCallback> cb = mTakePictureOnSuccessCb;
  if (cb) {
    if (aOnError.WasPassed()) {
      // There is already a call to TakePicture() in progress, abort this new
      // one and invoke the error callback (if one was passed in).
      NS_DispatchToMainThread(new ImmediateErrorCallback(&aOnError.Value(),
                              NS_LITERAL_STRING("TakePictureAlreadyInProgress")));
    }
    aRv = NS_ERROR_FAILURE;
    return;
  }

  {
    ICameraControlParameterSetAutoEnter batch(mCameraControl);

    // XXXmikeh - remove this: see bug 931155
    ICameraControl::Size s;
    s.width = aOptions.mPictureSize.mWidth;
    s.height = aOptions.mPictureSize.mHeight;

    ICameraControl::Position p;
    p.latitude = aOptions.mPosition.mLatitude;
    p.longitude = aOptions.mPosition.mLongitude;
    p.altitude = aOptions.mPosition.mAltitude;
    p.timestamp = aOptions.mPosition.mTimestamp;

    if (s.width && s.height) {
      mCameraControl->Set(CAMERA_PARAM_PICTURE_SIZE, s);
    }
    mCameraControl->Set(CAMERA_PARAM_PICTURE_ROTATION, aOptions.mRotation);
    mCameraControl->Set(CAMERA_PARAM_PICTURE_FILEFORMAT, aOptions.mFileFormat);
    mCameraControl->Set(CAMERA_PARAM_PICTURE_DATETIME, aOptions.mDateTime);
    mCameraControl->SetLocation(p);
  }

  mTakePictureOnSuccessCb = &aOnSuccess;
  mTakePictureOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mTakePictureOnErrorCb = &aOnError.Value();
  }

  aRv = mCameraControl->TakePicture();
}

void
nsDOMCameraControl::ReleaseHardware(const Optional<OwningNonNull<CameraReleaseCallback> >& aOnSuccess,
                                    const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  mReleaseOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mReleaseOnSuccessCb = &aOnSuccess.Value();
  }
  mReleaseOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mReleaseOnErrorCb = &aOnError.Value();
  }

  aRv = mCameraControl->Stop();
}

void
nsDOMCameraControl::ResumeContinuousFocus(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->ResumeContinuousFocus();
}

void
nsDOMCameraControl::Shutdown()
{
  DOM_CAMERA_LOGI("%s:%d\n", __func__, __LINE__);
  MOZ_ASSERT(mCameraControl);

  // Remove any pending solicited event handlers; these
  // reference our window object, which in turn references
  // us. If we don't remove them, we can leak DOM objects.
  mGetCameraOnSuccessCb = nullptr;
  mGetCameraOnErrorCb = nullptr;
  mAutoFocusOnSuccessCb = nullptr;
  mAutoFocusOnErrorCb = nullptr;
  mTakePictureOnSuccessCb = nullptr;
  mTakePictureOnErrorCb = nullptr;
  mStartRecordingOnSuccessCb = nullptr;
  mStartRecordingOnErrorCb = nullptr;
  mReleaseOnSuccessCb = nullptr;
  mReleaseOnErrorCb = nullptr;
  mSetConfigurationOnSuccessCb = nullptr;
  mSetConfigurationOnErrorCb = nullptr;

  // Remove all of the unsolicited event handlers too.
  mOnShutterCb = nullptr;
  mOnClosedCb = nullptr;
  mOnRecorderStateChangeCb = nullptr;
  mOnPreviewStateChangeCb = nullptr;
  mOnAutoFocusMovingCb = nullptr;
  mOnFacesDetectedCb = nullptr;

  mCameraControl->Shutdown();
}

nsresult
nsDOMCameraControl::NotifyRecordingStatusChange(const nsString& aMsg)
{
  NS_ENSURE_TRUE(mWindow, NS_ERROR_FAILURE);

  return MediaManager::NotifyRecordingStatusChange(mWindow,
                                                   aMsg,
                                                   true /* aIsAudio */,
                                                   true /* aIsVideo */);
}

// Camera Control event handlers--must only be called from the Main Thread!
void
nsDOMCameraControl::OnHardwareStateChange(CameraControlListener::HardwareState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult ignored;

  DOM_CAMERA_LOGI("DOM OnHardwareStateChange(%d)\n", aState);

  switch (aState) {
    case CameraControlListener::kHardwareOpen:
      // The hardware is open, so we can return a camera to JS, even if
      // the preview hasn't started yet.
      if (mGetCameraOnSuccessCb) {
        nsRefPtr<GetCameraCallback> cb = mGetCameraOnSuccessCb.forget();
        ErrorResult ignored;
        mGetCameraOnErrorCb = nullptr;
        cb->Call(*this, *mCurrentConfiguration, ignored);
      }
      break;

    case CameraControlListener::kHardwareClosed:
      if (mReleaseOnSuccessCb) {
        // If we have this event handler, this was a solicited hardware close.
        nsRefPtr<CameraReleaseCallback> cb = mReleaseOnSuccessCb.forget();
        mReleaseOnErrorCb = nullptr;
        cb->Call(ignored);
      } else if(mOnClosedCb) {
        // If not, something else closed the hardware.
        nsRefPtr<CameraClosedCallback> cb = mOnClosedCb;
        cb->Call(ignored);
      }
      break;

    default:
      MOZ_ASSUME_UNREACHABLE("Unanticipated camera hardware state");
  }
}

void
nsDOMCameraControl::OnShutter()
{
  MOZ_ASSERT(NS_IsMainThread());

  DOM_CAMERA_LOGI("DOM ** SNAP **\n");

  nsRefPtr<CameraShutterCallback> cb = mOnShutterCb;
  if (cb) {
    ErrorResult ignored;
    cb->Call(ignored);
  }
}

void
nsDOMCameraControl::OnPreviewStateChange(CameraControlListener::PreviewState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOnPreviewStateChangeCb) {
    return;
  }

  nsString state;
  switch (aState) {
    case CameraControlListener::kPreviewStarted:
      state = NS_LITERAL_STRING("started");
      break;

    default:
      state = NS_LITERAL_STRING("stopped");
      break;
  }

  nsRefPtr<CameraPreviewStateChange> cb = mOnPreviewStateChangeCb;
  ErrorResult ignored;
  cb->Call(state, ignored);
}

void
nsDOMCameraControl::OnRecorderStateChange(CameraControlListener::RecorderState aState,
                                          int32_t aArg, int32_t aTrackNum)
{
  // For now, we do nothing with 'aStatus' and 'aTrackNum'.
  MOZ_ASSERT(NS_IsMainThread());

  ErrorResult ignored;
  nsString state;

  switch (aState) {
    case CameraControlListener::kRecorderStarted:
      if (mStartRecordingOnSuccessCb) {
        nsRefPtr<CameraStartRecordingCallback> cb = mStartRecordingOnSuccessCb.forget();
        mStartRecordingOnErrorCb = nullptr;
        cb->Call(ignored);
      }
      state = NS_LITERAL_STRING("Started");
      break;

    case CameraControlListener::kRecorderStopped:
      NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));
      state = NS_LITERAL_STRING("Stopped");
      break;

#ifdef MOZ_B2G_CAMERA
    case CameraControlListener::kFileSizeLimitReached:
      state = NS_LITERAL_STRING("FileSizeLimitReached");
      break;

    case CameraControlListener::kVideoLengthLimitReached:
      state = NS_LITERAL_STRING("VideoLengthLimitReached");
      break;

    case CameraControlListener::kTrackCompleted:
      state = NS_LITERAL_STRING("TrackCompleted");
      break;

    case CameraControlListener::kTrackFailed:
      state = NS_LITERAL_STRING("TrackFailed");
      break;

    case CameraControlListener::kMediaRecorderFailed:
      state = NS_LITERAL_STRING("MediaRecorderFailed");
      break;

    case CameraControlListener::kMediaServerFailed:
      state = NS_LITERAL_STRING("MediaServerFailed");
      break;
#endif

    default:
      MOZ_ASSUME_UNREACHABLE("Unanticipated video recorder error");
      return;
  }

  nsRefPtr<CameraRecorderStateChange> cb = mOnRecorderStateChangeCb;
  if (cb) {
    cb->Call(state, ignored);
  }
}

void
nsDOMCameraControl::OnConfigurationChange(DOMCameraConfiguration* aConfiguration)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Update our record of the current camera configuration
  mCurrentConfiguration = aConfiguration;

  DOM_CAMERA_LOGI("DOM OnConfigurationChange: this=%p\n", this);
  DOM_CAMERA_LOGI("    mode                   : %s\n",
    mCurrentConfiguration->mMode == CameraMode::Video ? "video" : "picture");
  DOM_CAMERA_LOGI("    maximum focus areas    : %d\n",
    mCurrentConfiguration->mMaxFocusAreas);
  DOM_CAMERA_LOGI("    maximum metering areas : %d\n",
    mCurrentConfiguration->mMaxMeteringAreas);
  DOM_CAMERA_LOGI("    preview size (w x h)   : %d x %d\n",
    mCurrentConfiguration->mPreviewSize.mWidth, mCurrentConfiguration->mPreviewSize.mHeight);
  DOM_CAMERA_LOGI("    recorder profile       : %s\n",
    NS_ConvertUTF16toUTF8(mCurrentConfiguration->mRecorderProfile).get());

  nsRefPtr<CameraSetConfigurationCallback> cb = mSetConfigurationOnSuccessCb.forget();
  mSetConfigurationOnErrorCb = nullptr;
  if (cb) {
    ErrorResult ignored;
    cb->Call(*mCurrentConfiguration, ignored);
  }
}

void
nsDOMCameraControl::OnAutoFocusComplete(bool aAutoFocusSucceeded)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CameraAutoFocusCallback> cb = mAutoFocusOnSuccessCb.forget();
  mAutoFocusOnErrorCb = nullptr;
  if (cb) {
    ErrorResult ignored;
    cb->Call(aAutoFocusSucceeded, ignored);
  }
}

void
nsDOMCameraControl::OnAutoFocusMoving(bool aIsMoving)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CameraAutoFocusMovingCallback> cb = mOnAutoFocusMovingCb;
  if (cb) {
    ErrorResult ignored;
    cb->Call(aIsMoving, ignored);
  }
}

void
nsDOMCameraControl::OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces)
{
  DOM_CAMERA_LOGI("DOM OnFacesDetected %u face(s)\n", aFaces.Length());
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CameraFaceDetectionCallback> cb = mOnFacesDetectedCb;
  if (!cb) {
    return;
  }

  Sequence<OwningNonNull<DOMCameraDetectedFace> > faces;
  uint32_t len = aFaces.Length();

  if (faces.SetCapacity(len)) {
    nsRefPtr<DOMCameraDetectedFace> f;
    for (uint32_t i = 0; i < len; ++i) {
      f = new DOMCameraDetectedFace(this, aFaces[i]);
      *faces.AppendElement() = f.forget().take();
    }
  }

  ErrorResult ignored;
  cb->Call(faces, ignored);
}

void
nsDOMCameraControl::OnTakePictureComplete(nsIDOMBlob* aPicture)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CameraTakePictureCallback> cb = mTakePictureOnSuccessCb.forget();
  mTakePictureOnErrorCb = nullptr;
  if (!cb) {
    // Warn because it shouldn't be possible to get here without
    // having passed a success callback into takePicture(), even
    // though we guard against a nullptr dereference.
    NS_WARNING("DOM Null success callback in OnTakePictureComplete()");
    return;
  }

  ErrorResult ignored;
  cb->Call(aPicture, ignored);
}

void
nsDOMCameraControl::OnError(CameraControlListener::CameraErrorContext aContext, const nsAString& aError)
{
  DOM_CAMERA_LOGI("DOM OnError context=%d, error='%s'\n", aContext,
    NS_LossyConvertUTF16toASCII(aError).get());
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CameraErrorCallback> errorCb;

  switch (aContext) {
    case CameraControlListener::kInStartCamera:
      mGetCameraOnSuccessCb = nullptr;
      errorCb = mGetCameraOnErrorCb.forget();
      break;

    case CameraControlListener::kInStopCamera:
      mReleaseOnSuccessCb = nullptr;
      errorCb = mReleaseOnErrorCb.forget();
      break;

    case CameraControlListener::kInSetConfiguration:
      mSetConfigurationOnSuccessCb = nullptr;
      errorCb = mSetConfigurationOnErrorCb.forget();
      break;

    case CameraControlListener::kInAutoFocus:
      mAutoFocusOnSuccessCb = nullptr;
      errorCb = mAutoFocusOnErrorCb.forget();
      break;

    case CameraControlListener::kInTakePicture:
      mTakePictureOnSuccessCb = nullptr;
      errorCb = mTakePictureOnErrorCb.forget();
      break;

    case CameraControlListener::kInStartRecording:
      mStartRecordingOnSuccessCb = nullptr;
      errorCb = mStartRecordingOnErrorCb.forget();
      break;

    case CameraControlListener::kInStopRecording:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to stop recording");
      return;

    case CameraControlListener::kInStartPreview:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to (re)start preview");
      return;

    case CameraControlListener::kInUnspecified:
      if (aError.EqualsASCII("ErrorServiceFailed")) {
        // If the camera service fails, we will get preview-stopped and
        // hardware-closed events, so nothing to do here.
        NS_WARNING("Camera service failed");
        return;
      }
      if (aError.EqualsASCII("ErrorSetPictureSizeFailed") ||
          aError.EqualsASCII("ErrorSetThumbnailSizeFailed")) {
        // We currently don't handle attribute setter failure. Practically,
        // this only ever happens if a setter is called after the hardware
        // has gone away before an asynchronous set gets to happen, so we
        // swallow these.
        NS_WARNING("Failed to set either picture or thumbnail size");
        return;
      }
      // fallthrough

    default:
      MOZ_ASSUME_UNREACHABLE("Error occurred in unanticipated camera state");
      return;
  }

  if (!errorCb) {
    DOM_CAMERA_LOGW("DOM No error handler for error '%s' in context=%d\n",
      NS_LossyConvertUTF16toASCII(aError).get(), aContext);
    return;
  }

  ErrorResult ignored;
  errorCb->Call(aError, ignored);
}

