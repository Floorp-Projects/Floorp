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
#include "nsDOMFile.h"
#include "Navigator.h"
#include "nsXULAppAPI.h"
#include "DOMCameraManager.h"
#include "DOMCameraCapabilities.h"
#include "CameraCommon.h"
#include "nsGlobalWindow.h"
#include "CameraPreviewMediaStream.h"
#include "mozilla/dom/CameraUtilBinding.h"
#include "mozilla/dom/CameraControlBinding.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/CameraCapabilitiesBinding.h"
#include "mozilla/dom/CameraConfigurationEvent.h"
#include "mozilla/dom/CameraConfigurationEventBinding.h"
#include "mozilla/dom/CameraFacesDetectedEvent.h"
#include "mozilla/dom/CameraFacesDetectedEventBinding.h"
#include "mozilla/dom/CameraStateChangeEvent.h"
#include "mozilla/dom/BlobEvent.h"
#include "DOMCameraDetectedFace.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMCameraControl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

NS_IMPL_ADDREF_INHERITED(nsDOMCameraControl, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(nsDOMCameraControl, DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsDOMCameraControl, DOMMediaStream,
                                   mCapabilities,
                                   mWindow,
                                   mGetCameraPromise,
                                   mAutoFocusPromise,
                                   mTakePicturePromise,
                                   mStartRecordingPromise,
                                   mReleasePromise,
                                   mSetConfigurationPromise,
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
                                   mOnAutoFocusCompletedCb,
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

  explicit StartRecordingHelper(nsDOMCameraControl* aDOMCameraControl)
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

NS_IMPL_ISUPPORTS(mozilla::StartRecordingHelper, nsIDOMEventListener)

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
                                       Promise* aPromise,
                                       nsPIDOMWindow* aWindow)
  : DOMMediaStream()
  , mCameraControl(nullptr)
  , mAudioChannelAgent(nullptr)
  , mGetCameraPromise(aPromise)
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
  , mOnAutoFocusCompletedCb(nullptr)
  , mOnFacesDetectedCb(nullptr)
  , mWindow(aWindow)
  , mPreviewState(CameraControlListener::kPreviewStopped)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mInput = new CameraPreviewMediaStream(this);

  BindToOwner(aWindow);

  nsRefPtr<DOMCameraConfiguration> initialConfig =
    new DOMCameraConfiguration(aInitialConfig);

  // Create and initialize the underlying camera.
  ICameraControl::Configuration config;
  bool haveInitialConfig = false;
  nsresult rv;

  switch (aInitialConfig.mMode) {
    case CameraMode::Picture:
      config.mMode = ICameraControl::kPictureMode;
      haveInitialConfig = true;
      break;

    case CameraMode::Video:
      config.mMode = ICameraControl::kVideoMode;
      haveInitialConfig = true;
      break;

    case CameraMode::Unspecified:
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unanticipated camera mode!");
      break;
  }

  if (haveInitialConfig) {
    config.mPreviewSize.width = aInitialConfig.mPreviewSize.mWidth;
    config.mPreviewSize.height = aInitialConfig.mPreviewSize.mHeight;
    config.mRecorderProfile = aInitialConfig.mRecorderProfile;
  }

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
  if (haveInitialConfig) {
    rv = mCameraControl->Start(&config);
  } else {
    rv = mCameraControl->Start();
  }
  if (NS_FAILED(rv)) {
    mListener->OnUserError(DOMCameraControlListener::kInStartCamera, rv);
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

// Setter for weighted regions: { top, bottom, left, right, weight }
nsresult
nsDOMCameraControl::Set(uint32_t aKey, const Optional<Sequence<CameraRegion> >& aValue, uint32_t aLimit)
{
  if (aLimit == 0) {
    DOM_CAMERA_LOGI("%s:%d : aLimit = 0, nothing to do\n", __func__, __LINE__);
    return NS_OK;
  }

  nsTArray<ICameraControl::Region> regionArray;
  if (aValue.WasPassed()) {
    const Sequence<CameraRegion>& regions = aValue.Value();
    uint32_t length = regions.Length();

    DOM_CAMERA_LOGI("%s:%d : got %d regions (limited to %d)\n", __func__, __LINE__, length, aLimit);
    if (length > aLimit) {
      length = aLimit;
    }

    // aLimit supplied by camera library provides sane ceiling (i.e. <10)
    regionArray.SetCapacity(length);

    for (uint32_t i = 0; i < length; ++i) {
      ICameraControl::Region* r = regionArray.AppendElement();
      const CameraRegion &region = regions[i];
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
  } else {
    DOM_CAMERA_LOGI("%s:%d : clear regions\n", __func__, __LINE__);
  }
  return mCameraControl->Set(aKey, regionArray);
}

// Getter for weighted regions: { top, bottom, left, right, weight }
nsresult
nsDOMCameraControl::Get(uint32_t aKey, nsTArray<CameraRegion>& aValue)
{
  nsTArray<ICameraControl::Region> regionArray;

  nsresult rv = mCameraControl->Get(aKey, regionArray);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t length = regionArray.Length();
  DOM_CAMERA_LOGI("%s:%d : got %d regions\n", __func__, __LINE__, length);
  aValue.SetLength(length);

  for (uint32_t i = 0; i < length; ++i) {
    ICameraControl::Region& r = regionArray[i];
    CameraRegion& v = aValue[i];
    v.mTop = r.top;
    v.mLeft = r.left;
    v.mBottom = r.bottom;
    v.mRight = r.right;
    v.mWeight = r.weight;

    DOM_CAMERA_LOGI("region %d: top=%d, left=%d, bottom=%d, right=%d, weight=%u\n",
      i,
      v.mTop,
      v.mLeft,
      v.mBottom,
      v.mRight,
      v.mWeight
    );
  }

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
nsDOMCameraControl::GetPictureQuality(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double quality;
  aRv = mCameraControl->Get(CAMERA_PARAM_PICTURE_QUALITY, quality);
  return quality;
}
void
nsDOMCameraControl::SetPictureQuality(double aQuality, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURE_QUALITY, aQuality);
}

double
nsDOMCameraControl::GetZoom(ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  double zoom = 1.0;
  aRv = mCameraControl->Get(CAMERA_PARAM_ZOOM, zoom);
  return zoom;
}

void
nsDOMCameraControl::SetZoom(double aZoom, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_ZOOM, aZoom);
}

void
nsDOMCameraControl::GetMeteringAreas(nsTArray<CameraRegion>& aAreas, ErrorResult& aRv)
{
  aRv = Get(CAMERA_PARAM_METERINGAREAS, aAreas);
}
void
nsDOMCameraControl::SetMeteringAreas(const Optional<Sequence<CameraRegion> >& aMeteringAreas, ErrorResult& aRv)
{
  aRv = Set(CAMERA_PARAM_METERINGAREAS, aMeteringAreas,
            mCurrentConfiguration->mMaxMeteringAreas);
}

void
nsDOMCameraControl::GetFocusAreas(nsTArray<CameraRegion>& aAreas, ErrorResult& aRv)
{
  aRv = Get(CAMERA_PARAM_FOCUSAREAS, aAreas);
}
void
nsDOMCameraControl::SetFocusAreas(const Optional<Sequence<CameraRegion> >& aFocusAreas, ErrorResult& aRv)
{
  aRv = Set(CAMERA_PARAM_FOCUSAREAS, aFocusAreas,
            mCurrentConfiguration->mMaxFocusAreas);
}

void
nsDOMCameraControl::GetPictureSize(CameraSize& aSize, ErrorResult& aRv)
{
  ICameraControl::Size size;
  aRv = mCameraControl->Get(CAMERA_PARAM_PICTURE_SIZE, size);
  if (aRv.Failed()) {
    return;
  }

  aSize.mWidth = size.width;
  aSize.mHeight = size.height;
}

void
nsDOMCameraControl::SetPictureSize(const CameraSize& aSize, ErrorResult& aRv)
{
  ICameraControl::Size s = { aSize.mWidth, aSize.mHeight };
  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURE_SIZE, s);
}

void
nsDOMCameraControl::GetThumbnailSize(CameraSize& aSize, ErrorResult& aRv)
{
  ICameraControl::Size size;
  aRv = mCameraControl->Get(CAMERA_PARAM_THUMBNAILSIZE, size);
  if (aRv.Failed()) {
    return;
  }

  aSize.mWidth = size.width;
  aSize.mHeight = size.height;
}

void
nsDOMCameraControl::SetThumbnailSize(const CameraSize& aSize, ErrorResult& aRv)
{
  ICameraControl::Size s = { aSize.mWidth, aSize.mHeight };
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
nsDOMCameraControl::SetExposureCompensation(double aCompensation, ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);
  aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, aCompensation);
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

CameraAutoFocusCallback*
nsDOMCameraControl::GetOnAutoFocusCompleted()
{
  return mOnAutoFocusCompletedCb;
}
void
nsDOMCameraControl::SetOnAutoFocusCompleted(CameraAutoFocusCallback* aCb)
{
  mOnAutoFocusCompletedCb = aCb;
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

// Methods.
already_AddRefed<Promise>
nsDOMCameraControl::StartRecording(const CameraStartRecordingOptions& aOptions,
                                   nsDOMDeviceStorage& aStorageArea,
                                   const nsAString& aFilename,
                                   const Optional<OwningNonNull<CameraStartRecordingCallback> >& aOnSuccess,
                                   const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mStartRecordingPromise) {
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    if (aOnError.WasPassed()) {
      DOM_CAMERA_LOGT("%s:onError WasPassed\n", __func__);
      NS_DispatchToMainThread(new ImmediateErrorCallback(&aOnError.Value(),
                              NS_LITERAL_STRING("StartRecordingInProgress")));
    }
    return promise.forget();
  }

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
    return nullptr;
  }

  mStartRecordingPromise = promise;
  mOptions = aOptions;
  mStartRecordingOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mStartRecordingOnSuccessCb = &aOnSuccess.Value();
  }
  mStartRecordingOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mStartRecordingOnErrorCb = &aOnError.Value();
  }

  nsCOMPtr<nsIDOMEventListener> listener = new StartRecordingHelper(this);
  request->AddEventListener(NS_LITERAL_STRING("success"), listener, false);
  request->AddEventListener(NS_LITERAL_STRING("error"), listener, false);
  return promise.forget();
}

void
nsDOMCameraControl::OnCreatedFileDescriptor(bool aSucceeded)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aSucceeded && mDSFileDescriptor->mFileDescriptor.IsValid()) {
    ICameraControl::StartRecordingOptions o;

    o.rotation = mOptions.mRotation;
    o.maxFileSizeBytes = mOptions.mMaxFileSizeBytes;
    o.maxVideoLengthMs = mOptions.mMaxVideoLengthMs;
    o.autoEnableLowLightTorch = mOptions.mAutoEnableLowLightTorch;
    rv = mCameraControl->StartRecording(mDSFileDescriptor.get(), &o);
    if (NS_SUCCEEDED(rv)) {
      return;
    }
  }

  OnUserError(CameraControlListener::kInStartRecording, rv);

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

already_AddRefed<Promise>
nsDOMCameraControl::SetConfiguration(const CameraConfiguration& aConfiguration,
                                     const Optional<OwningNonNull<CameraSetConfigurationCallback> >& aOnSuccess,
                                     const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mTakePicturePromise) {
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    // We're busy taking a picture, can't change modes right now.
    if (aOnError.WasPassed()) {
      // There is already a call to TakePicture() in progress, abort this
      // call and invoke the error callback (if one was passed in).
      NS_DispatchToMainThread(new ImmediateErrorCallback(&aOnError.Value(),
                              NS_LITERAL_STRING("TakePictureInProgress")));
    }
    return promise.forget();
  }

  ICameraControl::Configuration config;
  config.mRecorderProfile = aConfiguration.mRecorderProfile;
  config.mPreviewSize.width = aConfiguration.mPreviewSize.mWidth;
  config.mPreviewSize.height = aConfiguration.mPreviewSize.mHeight;
  config.mMode = ICameraControl::kPictureMode;
  if (aConfiguration.mMode == CameraMode::Video) {
    config.mMode = ICameraControl::kVideoMode;
  }

  aRv = mCameraControl->SetConfiguration(config);
  if (aRv.Failed()) {
    return nullptr;
  }

  mSetConfigurationPromise = promise;
  mSetConfigurationOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mSetConfigurationOnSuccessCb = &aOnSuccess.Value();
  }
  mSetConfigurationOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mSetConfigurationOnErrorCb = &aOnError.Value();
  }
  return promise.forget();
}

already_AddRefed<Promise>
nsDOMCameraControl::AutoFocus(const Optional<OwningNonNull<CameraAutoFocusCallback> >& aOnSuccess,
                              const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                              ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<Promise> promise = mAutoFocusPromise.forget();
  if (promise) {
    // There is already a call to AutoFocus() in progress, cancel it and
    // invoke the error callback (if one was passed in).
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    mAutoFocusOnSuccessCb = nullptr;
    nsRefPtr<CameraErrorCallback> ecb = mAutoFocusOnErrorCb.forget();
    if (ecb) {
      NS_DispatchToMainThread(new ImmediateErrorCallback(ecb,
                              NS_LITERAL_STRING("AutoFocusInterrupted")));
    }
  }

  promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = mCameraControl->AutoFocus();
  if (aRv.Failed()) {
    return nullptr;
  }

  DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("focusing"));

  mAutoFocusPromise = promise;
  mAutoFocusOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mAutoFocusOnSuccessCb = &aOnSuccess.Value();
  }
  mAutoFocusOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mAutoFocusOnErrorCb = &aOnError.Value();
  }
  return promise.forget();
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

already_AddRefed<Promise>
nsDOMCameraControl::TakePicture(const CameraPictureOptions& aOptions,
                                const Optional<OwningNonNull<CameraTakePictureCallback> >& aOnSuccess,
                                const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mTakePicturePromise) {
    // There is already a call to TakePicture() in progress, abort this new
    // one and invoke the error callback (if one was passed in).
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    if (aOnError.WasPassed()) {
      NS_DispatchToMainThread(new ImmediateErrorCallback(&aOnError.Value(),
                              NS_LITERAL_STRING("TakePictureAlreadyInProgress")));
    }
    return promise.forget();
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

  aRv = mCameraControl->TakePicture();
  if (aRv.Failed()) {
    return nullptr;
  }

  mTakePicturePromise = promise;
  mTakePictureOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mTakePictureOnSuccessCb = &aOnSuccess.Value();
  }
  mTakePictureOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mTakePictureOnErrorCb = &aOnError.Value();
  }
  return promise.forget();
}

already_AddRefed<Promise>
nsDOMCameraControl::ReleaseHardware(const Optional<OwningNonNull<CameraReleaseCallback> >& aOnSuccess,
                                    const Optional<OwningNonNull<CameraErrorCallback> >& aOnError,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(mCameraControl);

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = mCameraControl->Stop();
  if (aRv.Failed()) {
    return nullptr;
  }

  mReleasePromise = promise;
  mReleaseOnSuccessCb = nullptr;
  if (aOnSuccess.WasPassed()) {
    mReleaseOnSuccessCb = &aOnSuccess.Value();
  }
  mReleaseOnErrorCb = nullptr;
  if (aOnError.WasPassed()) {
    mReleaseOnErrorCb = &aOnError.Value();
  }
  return promise.forget();
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
  AbortPromise(mGetCameraPromise);
  AbortPromise(mAutoFocusPromise);
  AbortPromise(mTakePicturePromise);
  AbortPromise(mStartRecordingPromise);
  AbortPromise(mReleasePromise);
  AbortPromise(mSetConfigurationPromise);
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
  mOnAutoFocusCompletedCb = nullptr;
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

already_AddRefed<Promise>
nsDOMCameraControl::CreatePromise(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  return Promise::Create(global, aRv);
}

void
nsDOMCameraControl::AbortPromise(nsRefPtr<Promise>& aPromise)
{
  nsRefPtr<Promise> promise = aPromise.forget();
  if (promise) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  }
}

void
nsDOMCameraControl::EventListenerAdded(nsIAtom* aType)
{
  if (aType == nsGkAtoms::onpreviewstatechange) {
    DispatchPreviewStateEvent(mPreviewState);
  }
}

void
nsDOMCameraControl::DispatchPreviewStateEvent(CameraControlListener::PreviewState aState)
{
  nsString state;
  switch (aState) {
    case CameraControlListener::kPreviewStarted:
      state = NS_LITERAL_STRING("started");
      break;

    default:
      state = NS_LITERAL_STRING("stopped");
      break;
  }

  DispatchStateEvent(NS_LITERAL_STRING("previewstatechange"), state);
}

void
nsDOMCameraControl::DispatchStateEvent(const nsString& aType, const nsString& aState)
{
  CameraStateChangeEventInit eventInit;
  eventInit.mNewState = aState;

  nsRefPtr<CameraStateChangeEvent> event =
    CameraStateChangeEvent::Constructor(this, aType, eventInit);

  DispatchTrustedEvent(event);
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
      {
        // The hardware is open, so we can return a camera to JS, even if
        // the preview hasn't started yet.
        nsRefPtr<Promise> promise = mGetCameraPromise.forget();
        if (promise) {
          CameraGetPromiseData data;
          data.mCamera = this;
          data.mConfiguration = *mCurrentConfiguration;
          promise->MaybeResolve(data);
        }
        nsRefPtr<GetCameraCallback> cb = mGetCameraOnSuccessCb.forget();
        mGetCameraOnErrorCb = nullptr;
        if (cb) {
          ErrorResult ignored;
          cb->Call(*this, *mCurrentConfiguration, ignored);
        }
      }
      break;

    case CameraControlListener::kHardwareClosed:
      {
        nsRefPtr<Promise> promise = mReleasePromise.forget();
        if (promise || mReleaseOnSuccessCb) {
          // If we have this event handler, this was a solicited hardware close.
          if (promise) {
            promise->MaybeResolve(JS::UndefinedHandleValue);
          }
          nsRefPtr<CameraReleaseCallback> cb = mReleaseOnSuccessCb.forget();
          mReleaseOnErrorCb = nullptr;
          if (cb) {
            cb->Call(ignored);
          }
        } else {
          // If not, something else closed the hardware.
          nsRefPtr<CameraClosedCallback> cb = mOnClosedCb;
          if (cb) {
            cb->Call(ignored);
          }
        }
        DispatchTrustedEvent(NS_LITERAL_STRING("close"));
      }
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unanticipated camera hardware state");
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

  DispatchTrustedEvent(NS_LITERAL_STRING("shutter"));
}

void
nsDOMCameraControl::OnPreviewStateChange(CameraControlListener::PreviewState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  mPreviewState = aState;
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
  if (cb) {
    ErrorResult ignored;
    cb->Call(state, ignored);
  }

  DispatchPreviewStateEvent(aState);
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
      {
        nsRefPtr<Promise> promise = mStartRecordingPromise.forget();
        if (promise) {
          promise->MaybeResolve(JS::UndefinedHandleValue);
        }

        nsRefPtr<CameraStartRecordingCallback> scb = mStartRecordingOnSuccessCb.forget();
        mStartRecordingOnErrorCb = nullptr;
        if (scb) {
          scb->Call(ignored);
        }
        state = NS_LITERAL_STRING("Started");
      }
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
      MOZ_ASSERT_UNREACHABLE("Unanticipated video recorder error");
      return;
  }

  nsRefPtr<CameraRecorderStateChange> cb = mOnRecorderStateChangeCb;
  if (cb) {
    cb->Call(state, ignored);
  }

  DispatchStateEvent(NS_LITERAL_STRING("recorderstatechange"), state);
}

void
nsDOMCameraControl::OnConfigurationChange(DOMCameraConfiguration* aConfiguration)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aConfiguration != nullptr);

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

  nsRefPtr<Promise> promise = mSetConfigurationPromise.forget();
  if (promise) {
    promise->MaybeResolve(*aConfiguration);
  }

  nsRefPtr<CameraSetConfigurationCallback> cb = mSetConfigurationOnSuccessCb.forget();
  mSetConfigurationOnErrorCb = nullptr;
  if (cb) {
    ErrorResult ignored;
    cb->Call(*mCurrentConfiguration, ignored);
  }

  CameraConfigurationEventInit eventInit;
  eventInit.mMode = mCurrentConfiguration->mMode;
  eventInit.mRecorderProfile = mCurrentConfiguration->mRecorderProfile;
  eventInit.mPreviewSize = new DOMRect(this, 0, 0,
                                       mCurrentConfiguration->mPreviewSize.mWidth,
                                       mCurrentConfiguration->mPreviewSize.mHeight);

  nsRefPtr<CameraConfigurationEvent> event =
    CameraConfigurationEvent::Constructor(this,
                                          NS_LITERAL_STRING("configurationchanged"),
                                          eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnAutoFocusComplete(bool aAutoFocusSucceeded)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<Promise> promise = mAutoFocusPromise.forget();
  if (promise) {
    promise->MaybeResolve(aAutoFocusSucceeded);
  }

  nsRefPtr<CameraAutoFocusCallback> cb = mAutoFocusOnSuccessCb.forget();
  mAutoFocusOnErrorCb = nullptr;
  if (cb) {
    ErrorResult ignored;
    cb->Call(aAutoFocusSucceeded, ignored);
  }

  cb = mOnAutoFocusCompletedCb;
  if (cb) {
    ErrorResult ignored;
    cb->Call(aAutoFocusSucceeded, ignored);
  }

  if (aAutoFocusSucceeded) {
    DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("focused"));
  } else {
    DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("unfocused"));
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

  if (aIsMoving) {
    DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("focusing"));
  }
}

void
nsDOMCameraControl::OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces)
{
  DOM_CAMERA_LOGI("DOM OnFacesDetected %u face(s)\n", aFaces.Length());
  MOZ_ASSERT(NS_IsMainThread());

  Sequence<OwningNonNull<DOMCameraDetectedFace> > faces;
  uint32_t len = aFaces.Length();

  if (faces.SetCapacity(len)) {
    nsRefPtr<DOMCameraDetectedFace> f;
    for (uint32_t i = 0; i < len; ++i) {
      f = new DOMCameraDetectedFace(this, aFaces[i]);
      *faces.AppendElement() = f.forget().take();
    }
  }

  nsRefPtr<CameraFaceDetectionCallback> cb = mOnFacesDetectedCb;
  if (cb) {
    ErrorResult ignored;
    cb->Call(faces, ignored);
  }

  CameraFacesDetectedEventInit eventInit;
  eventInit.mFaces.SetValue(faces);

  nsRefPtr<CameraFacesDetectedEvent> event =
    CameraFacesDetectedEvent::Constructor(this,
                                          NS_LITERAL_STRING("facesdetected"),
                                          eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnTakePictureComplete(nsIDOMBlob* aPicture)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPicture);

  nsRefPtr<Promise> promise = mTakePicturePromise.forget();
  if (promise) {
    nsCOMPtr<nsIDOMBlob> picture = aPicture;
    promise->MaybeResolve(picture);
  }

  nsRefPtr<DOMFile> blob = static_cast<DOMFile*>(aPicture);

  nsRefPtr<CameraTakePictureCallback> cb = mTakePictureOnSuccessCb.forget();
  mTakePictureOnErrorCb = nullptr;
  if (cb) {
    ErrorResult ignored;
    cb->Call(*blob, ignored);
  }

  BlobEventInit eventInit;
  eventInit.mData = blob;

  nsRefPtr<BlobEvent> event = BlobEvent::Constructor(this,
                                                     NS_LITERAL_STRING("picture"),
                                                     eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnUserError(CameraControlListener::UserContext aContext, nsresult aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<Promise> promise;
  nsRefPtr<CameraErrorCallback> errorCb;

  switch (aContext) {
    case CameraControlListener::kInStartCamera:
      promise = mGetCameraPromise.forget();
      mGetCameraOnSuccessCb = nullptr;
      errorCb = mGetCameraOnErrorCb.forget();
      break;

    case CameraControlListener::kInStopCamera:
      promise = mReleasePromise.forget();
      mReleaseOnSuccessCb = nullptr;
      errorCb = mReleaseOnErrorCb.forget();
      break;

    case CameraControlListener::kInSetConfiguration:
      promise = mSetConfigurationPromise.forget();
      mSetConfigurationOnSuccessCb = nullptr;
      errorCb = mSetConfigurationOnErrorCb.forget();
      break;

    case CameraControlListener::kInAutoFocus:
      promise = mAutoFocusPromise.forget();
      mAutoFocusOnSuccessCb = nullptr;
      errorCb = mAutoFocusOnErrorCb.forget();
      DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("unfocused"));
      break;

    case CameraControlListener::kInTakePicture:
      promise = mTakePicturePromise.forget();
      mTakePictureOnSuccessCb = nullptr;
      errorCb = mTakePictureOnErrorCb.forget();
      break;

    case CameraControlListener::kInStartRecording:
      promise = mStartRecordingPromise.forget();
      mStartRecordingOnSuccessCb = nullptr;
      errorCb = mStartRecordingOnErrorCb.forget();
      break;

    case CameraControlListener::kInStartFaceDetection:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to start face detection");
      return;

    case CameraControlListener::kInStopFaceDetection:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to stop face detection");
      return;

    case CameraControlListener::kInResumeContinuousFocus:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to resume continuous focus");
      return;

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

    case CameraControlListener::kInStopPreview:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to stop preview");
      return;

    case CameraControlListener::kInSetPictureSize:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to set picture size");
      return;

    case CameraControlListener::kInSetThumbnailSize:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to set thumbnail size");
      return;

    default:
      {
        nsPrintfCString msg("Unhandled aContext=%u, aError=0x%x\n", aContext, aError);
        NS_WARNING(msg.get());
      }
      MOZ_ASSERT_UNREACHABLE("Unhandled user error");
      return;
  }

  if (!promise && !errorCb) {
    DOM_CAMERA_LOGW("DOM No error handler for aError=0x%x in aContext=%u\n",
      aError, aContext);
    return;
  }

  DOM_CAMERA_LOGI("DOM OnUserError aContext=%u, aError=0x%x\n", aContext, aError);
  if (promise) {
    promise->MaybeReject(aError);
  }

  if (errorCb) {
    nsString error;
    switch (aError) {
      case NS_ERROR_INVALID_ARG:
        error = NS_LITERAL_STRING("InvalidArgument");
        break;

      case NS_ERROR_NOT_AVAILABLE:
        error = NS_LITERAL_STRING("NotAvailable");
        break;

      case NS_ERROR_NOT_IMPLEMENTED:
        error = NS_LITERAL_STRING("NotImplemented");
        break;

      case NS_ERROR_NOT_INITIALIZED:
        error = NS_LITERAL_STRING("HardwareClosed");
        break;

      case NS_ERROR_ALREADY_INITIALIZED:
        error = NS_LITERAL_STRING("HardwareAlreadyOpen");
        break;

      case NS_ERROR_OUT_OF_MEMORY:
        error = NS_LITERAL_STRING("OutOfMemory");
        break;

      default:
        {
          nsPrintfCString msg("Reporting aError=0x%x as generic\n", aError);
          NS_WARNING(msg.get());
        }
        // fallthrough

      case NS_ERROR_FAILURE:
        error = NS_LITERAL_STRING("GeneralFailure");
        break;
    }

    ErrorResult ignored;
    errorCb->Call(error, ignored);
  }
}
