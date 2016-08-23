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
#include "mozilla/dom/File.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsIAppsService.h"
#include "nsIObserverService.h"
#include "nsIDOMEventListener.h"
#include "nsIScriptSecurityManager.h"
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
#include "mozilla/dom/CameraClosedEvent.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/BlobEvent.h"
#include "DOMCameraDetectedFace.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

class mozilla::TrackCreatedListener : public MediaStreamListener
{
public:
  explicit TrackCreatedListener(nsDOMCameraControl* aCameraControl)
    : mCameraControl(aCameraControl) {}

  void Forget() { mCameraControl = nullptr; }

  void DoNotifyTrackCreated(TrackID aTrackID)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mCameraControl) {
      return;
    }

    mCameraControl->TrackCreated(aTrackID);
  }

  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                StreamTime aTrackOffset, TrackEventCommand aTrackEvents,
                                const MediaSegment& aQueuedMedia,
                                MediaStream* aInputStream,
                                TrackID aInputTrackID) override
  {
    if (aTrackEvents & TrackEventCommand::TRACK_EVENT_CREATED) {
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(NewRunnableMethod<TrackID>(
          this, &TrackCreatedListener::DoNotifyTrackCreated, aID));
    }
  }

protected:
  ~TrackCreatedListener() {}

  nsDOMCameraControl* mCameraControl;
};

#ifdef MOZ_WIDGET_GONK
StaticRefPtr<ICameraControl> nsDOMCameraControl::sCachedCameraControl;
/* static */ nsresult nsDOMCameraControl::sCachedCameraControlStartResult = NS_OK;
/* static */ nsCOMPtr<nsITimer> nsDOMCameraControl::sDiscardCachedCameraControlTimer;
#endif

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMCameraControl)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  // nsISupports is an ambiguous base of nsDOMCameraControl
  // so we have to work around that.
  if ( aIID.Equals(NS_GET_IID(nsDOMCameraControl)) )
    foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
  else
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

NS_IMPL_ADDREF_INHERITED(nsDOMCameraControl, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(nsDOMCameraControl, DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsDOMCameraControl, DOMMediaStream,
                                   mAudioChannelAgent,
                                   mCapabilities,
                                   mWindow,
                                   mGetCameraPromise,
                                   mAutoFocusPromise,
                                   mTakePicturePromise,
                                   mStartRecordingPromise,
                                   mReleasePromise,
                                   mSetConfigurationPromise)

/* static */
bool
nsDOMCameraControl::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

static nsresult
RegisterStorageRequestEvents(DOMRequest* aRequest, nsIDOMEventListener* aListener)
{
  EventListenerManager* elm = aRequest->GetOrCreateListenerManager();
  if (NS_WARN_IF(!elm)) {
    return NS_ERROR_UNEXPECTED;
  }

  elm->AddEventListener(NS_LITERAL_STRING("success"), aListener, false, false);
  elm->AddEventListener(NS_LITERAL_STRING("error"), aListener, false, false);
  return NS_OK;
}

class mozilla::StartRecordingHelper : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit StartRecordingHelper(nsDOMCameraControl* aDOMCameraControl)
    : mDOMCameraControl(aDOMCameraControl)
    , mState(false)
  {
    MOZ_COUNT_CTOR(StartRecordingHelper);
  }

protected:
  virtual ~StartRecordingHelper()
  {
    MOZ_COUNT_DTOR(StartRecordingHelper);
    mDOMCameraControl->OnCreatedFileDescriptor(mState);
  }

protected:
  RefPtr<nsDOMCameraControl> mDOMCameraControl;
  bool mState;
};

NS_IMETHODIMP
StartRecordingHelper::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString eventType;
  aEvent->GetType(eventType);
  mState = eventType.EqualsLiteral("success");
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

#ifdef MOZ_WIDGET_GONK
// This should be long enough for even our slowest platforms.
static const unsigned long kCachedCameraTimeoutMs = 3500;

// Open the battery-door-facing camera by default.
static const uint32_t kDefaultCameraId = 0;

/* static */ void
nsDOMCameraControl::PreinitCameraHardware()
{
  // Assume a default, minimal configuration. This should initialize the
  // hardware, but won't (can't) start the preview.
  RefPtr<ICameraControl> cameraControl = ICameraControl::Create(kDefaultCameraId);
  if (NS_WARN_IF(!cameraControl)) {
    return;
  }

  sCachedCameraControlStartResult = cameraControl->Start();
  if (NS_WARN_IF(NS_FAILED(sCachedCameraControlStartResult))) {
    return;
  }

  sCachedCameraControl = cameraControl;

  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (NS_WARN_IF(!timer)) {
    return;
  }

  nsresult rv = timer->InitWithFuncCallback(DiscardCachedCameraInstance,
                                            nullptr,
                                            kCachedCameraTimeoutMs,
                                            nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we can't start the timer, it's possible for an app to never grab the
    // camera, leaving the hardware tied up indefinitely. Better to take the
    // performance hit.
    sCachedCameraControl = nullptr;
    return;
  }

  sDiscardCachedCameraControlTimer = timer;
}

/* static */ void
nsDOMCameraControl::DiscardCachedCameraInstance(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());

  sDiscardCachedCameraControlTimer = nullptr;
  sCachedCameraControl = nullptr;
}
#endif

nsDOMCameraControl::nsDOMCameraControl(uint32_t aCameraId,
                                       const CameraConfiguration& aInitialConfig,
                                       Promise* aPromise,
                                       nsPIDOMWindowInner* aWindow)
  : DOMMediaStream(aWindow, nullptr)
  , mCameraControl(nullptr)
  , mAudioChannelAgent(nullptr)
  , mGetCameraPromise(aPromise)
  , mWindow(aWindow)
  , mPreviewState(CameraControlListener::kPreviewStopped)
  , mRecording(false)
  , mRecordingStoppedDeferred(false)
  , mSetInitialConfig(false)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mInput = new CameraPreviewMediaStream();
  mOwnedStream = mInput;

  BindToOwner(aWindow);

  RefPtr<DOMCameraConfiguration> initialConfig =
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
    rv = SelectPreviewSize(aInitialConfig.mPreviewSize, config.mPreviewSize);
    if (NS_FAILED(rv)) {
      mListener->OnUserError(DOMCameraControlListener::kInStartCamera, rv);
      return;
    }

    config.mPictureSize.width = aInitialConfig.mPictureSize.mWidth;
    config.mPictureSize.height = aInitialConfig.mPictureSize.mHeight;
    config.mRecorderProfile = aInitialConfig.mRecorderProfile;
  }

#ifdef MOZ_WIDGET_GONK
  bool gotCached = false;
  if (sCachedCameraControl && aCameraId == kDefaultCameraId) {
    mCameraControl = sCachedCameraControl;
    sCachedCameraControl = nullptr;
    gotCached = true;
  } else {
    sCachedCameraControl = nullptr;
#endif
    mCameraControl = ICameraControl::Create(aCameraId);
#ifdef MOZ_WIDGET_GONK
  }
#endif
  mCurrentConfiguration = initialConfig.forget();

  // Register a TrackCreatedListener directly on CameraPreviewMediaStream
  // so we can know the TrackID of the video track.
  mTrackCreatedListener = new TrackCreatedListener(this);
  mInput->AddListener(mTrackCreatedListener);

  // Register the playback listener directly on the camera input stream.
  // We want as low latency as possible for the camera, thus avoiding
  // MediaStreamGraph altogether. Don't do the regular InitStreamCommon()
  // to avoid initializing the Owned and Playback streams. This is OK since
  // we are not user/DOM facing anyway.
  CreateAndAddPlaybackStreamListener(mInput);

  // Register a listener for camera events.
  mListener = new DOMCameraControlListener(this, mInput);
  mCameraControl->AddListener(mListener);

#ifdef MOZ_WIDGET_GONK
  if (!gotCached || NS_FAILED(sCachedCameraControlStartResult)) {
#endif
    // Start the camera...
    if (haveInitialConfig) {
      rv = mCameraControl->Start(&config);
      if (NS_SUCCEEDED(rv)) {
        mSetInitialConfig = true;
      }
    } else {
      rv = mCameraControl->Start();
    }
#ifdef MOZ_WIDGET_GONK
  } else {
    if (haveInitialConfig) {
      rv = mCameraControl->SetConfiguration(config);
      if (NS_SUCCEEDED(rv)) {
        mSetInitialConfig = true;
      }
    } else {
      rv = NS_OK;
    }
  }
#endif
  if (NS_FAILED(rv)) {
    mListener->OnUserError(DOMCameraControlListener::kInStartCamera, rv);
  }
}

nsDOMCameraControl::~nsDOMCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  /*invoke DOMMediaStream destroy*/
  Destroy();

  if (mInput) {
    mInput->Destroy();
    mInput = nullptr;
  }
  if (mTrackCreatedListener) {
    mTrackCreatedListener->Forget();
    mTrackCreatedListener = nullptr;
  }
}

JSObject*
nsDOMCameraControl::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraControlBinding::Wrap(aCx, this, aGivenProto);
}

bool
nsDOMCameraControl::IsWindowStillActive()
{
  return nsDOMCameraManager::IsWindowStillActive(mWindow->WindowID());
}

nsresult
nsDOMCameraControl::SelectPreviewSize(const CameraSize& aRequestedPreviewSize, ICameraControl::Size& aSelectedPreviewSize)
{
  if (aRequestedPreviewSize.mWidth && aRequestedPreviewSize.mHeight) {
    aSelectedPreviewSize.width = aRequestedPreviewSize.mWidth;
    aSelectedPreviewSize.height = aRequestedPreviewSize.mHeight;
  } else {
    /* Use the window width and height if no preview size is provided.
       Note that the width and height are actually reversed from the
       camera perspective. */
    int32_t width = 0;
    int32_t height = 0;
    float ratio = 0.0;
    nsresult rv;

    rv = mWindow->GetDevicePixelRatio(&ratio);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mWindow->GetInnerWidth(&height);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mWindow->GetInnerHeight(&width);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(width > 0);
    MOZ_ASSERT(height > 0);
    MOZ_ASSERT(ratio > 0.0);
    aSelectedPreviewSize.width = std::ceil(width * ratio);
    aSelectedPreviewSize.height = std::ceil(height * ratio);
  }

  return NS_OK;
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

MediaStream*
nsDOMCameraControl::GetCameraStream() const
{
  return mInput;
}

void
nsDOMCameraControl::TrackCreated(TrackID aTrackID) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWindow, "Shouldn't have been created with a null window!");
  nsIPrincipal* principal = mWindow->GetExtantDoc()
                          ? mWindow->GetExtantDoc()->NodePrincipal()
                          : nullptr;

  // This track is not connected through a port.
  MediaInputPort* inputPort = nullptr;
  dom::VideoStreamTrack* track =
    new dom::VideoStreamTrack(this, aTrackID, aTrackID,
                              new BasicUnstoppableTrackSource(principal));
  RefPtr<TrackPort> port =
    new TrackPort(inputPort, track,
                  TrackPort::InputPortOwnership::OWNED);
  mTracks.AppendElement(port.forget());
  NotifyTrackAdded(track);
}

#define THROW_IF_NO_CAMERACONTROL(...)                                          \
  do {                                                                          \
    if (!mCameraControl) {                                                      \
      DOM_CAMERA_LOGW("mCameraControl is null at %s:%d\n", __func__, __LINE__); \
      aRv = NS_ERROR_NOT_AVAILABLE;                                             \
      return __VA_ARGS__;                                                       \
    }                                                                           \
  } while (0)

void
nsDOMCameraControl::GetEffect(nsString& aEffect, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_EFFECT, aEffect);
}
void
nsDOMCameraControl::SetEffect(const nsAString& aEffect, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_EFFECT, aEffect);
}

void
nsDOMCameraControl::GetWhiteBalanceMode(nsString& aWhiteBalanceMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}
void
nsDOMCameraControl::SetWhiteBalanceMode(const nsAString& aWhiteBalanceMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}

void
nsDOMCameraControl::GetSceneMode(nsString& aSceneMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_SCENEMODE, aSceneMode);
}
void
nsDOMCameraControl::SetSceneMode(const nsAString& aSceneMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_SCENEMODE, aSceneMode);
}

void
nsDOMCameraControl::GetFlashMode(nsString& aFlashMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_FLASHMODE, aFlashMode);
}
void
nsDOMCameraControl::SetFlashMode(const nsAString& aFlashMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_FLASHMODE, aFlashMode);
}

void
nsDOMCameraControl::GetFocusMode(nsString& aFocusMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}
void
nsDOMCameraControl::SetFocusMode(const nsAString& aFocusMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}

void
nsDOMCameraControl::GetIsoMode(nsString& aIsoMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_ISOMODE, aIsoMode);
}
void
nsDOMCameraControl::SetIsoMode(const nsAString& aIsoMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_ISOMODE, aIsoMode);
}

double
nsDOMCameraControl::GetPictureQuality(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(1.0);

  double quality;
  aRv = mCameraControl->Get(CAMERA_PARAM_PICTURE_QUALITY, quality);
  return quality;
}
void
nsDOMCameraControl::SetPictureQuality(double aQuality, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURE_QUALITY, aQuality);
}

void
nsDOMCameraControl::GetMeteringMode(nsString& aMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Get(CAMERA_PARAM_METERINGMODE, aMode);
}
void
nsDOMCameraControl::SetMeteringMode(const nsAString& aMode, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_METERINGMODE, aMode);
}

double
nsDOMCameraControl::GetZoom(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(1.0);

  double zoom = 1.0;
  aRv = mCameraControl->Get(CAMERA_PARAM_ZOOM, zoom);
  return zoom;
}

void
nsDOMCameraControl::SetZoom(double aZoom, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
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
  THROW_IF_NO_CAMERACONTROL();

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
  THROW_IF_NO_CAMERACONTROL();
  ICameraControl::Size s = { aSize.mWidth, aSize.mHeight };
  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURE_SIZE, s);
}

void
nsDOMCameraControl::GetThumbnailSize(CameraSize& aSize, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
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
  THROW_IF_NO_CAMERACONTROL();
  ICameraControl::Size s = { aSize.mWidth, aSize.mHeight };
  aRv = mCameraControl->Set(CAMERA_PARAM_THUMBNAILSIZE, s);
}

double
nsDOMCameraControl::GetFocalLength(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(0.0);

  double focalLength;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCALLENGTH, focalLength);
  return focalLength;
}

double
nsDOMCameraControl::GetFocusDistanceNear(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(0.0);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCENEAR, distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceOptimum(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(0.0);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEOPTIMUM, distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceFar(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(0.0);

  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEFAR, distance);
  return distance;
}

void
nsDOMCameraControl::SetExposureCompensation(double aCompensation, ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, aCompensation);
}

double
nsDOMCameraControl::GetExposureCompensation(ErrorResult& aRv)
{
  THROW_IF_NO_CAMERACONTROL(0.0);

  double compensation;
  aRv = mCameraControl->Get(CAMERA_PARAM_EXPOSURECOMPENSATION, compensation);
  return compensation;
}

int32_t
nsDOMCameraControl::SensorAngle()
{
  int32_t angle = 0;
  if (mCameraControl) {
    mCameraControl->Get(CAMERA_PARAM_SENSORANGLE, angle);
  }
  return angle;
}

already_AddRefed<dom::CameraCapabilities>
nsDOMCameraControl::Capabilities()
{
  if (!mCameraControl) {
    DOM_CAMERA_LOGW("mCameraControl is null at %s:%d\n", __func__, __LINE__);
    return nullptr;
  }

  RefPtr<CameraCapabilities> caps = mCapabilities;
  if (!caps) {
    caps = new CameraCapabilities(mWindow, mCameraControl);
    mCapabilities = caps;
  }

  return caps.forget();
}

// Methods.
already_AddRefed<Promise>
nsDOMCameraControl::StartRecording(const CameraStartRecordingOptions& aOptions,
                                   nsDOMDeviceStorage& aStorageArea,
                                   const nsAString& aFilename,
                                   ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If we are trying to start recording, already recording or are still
  // waiting for a poster to be created/fail, we need to wait
  if (mStartRecordingPromise || mRecording ||
      mRecordingStoppedDeferred ||
      mOptions.mCreatePoster) {
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    return promise.forget();
  }

  aRv = NotifyRecordingStatusChange(NS_LITERAL_STRING("starting"));
  if (aRv.Failed()) {
    return nullptr;
  }

  mDSFileDescriptor = new DeviceStorageFileDescriptor();
  RefPtr<DOMRequest> request = aStorageArea.CreateFileDescriptor(aFilename,
                                                                   mDSFileDescriptor.get(),
                                                                   aRv);
  if (aRv.Failed()) {
    NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));
    return nullptr;
  }

  nsCOMPtr<nsIDOMEventListener> listener = new StartRecordingHelper(this);
  aRv = RegisterStorageRequestEvents(request, listener);
  if (aRv.Failed()) {
    NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));
    return nullptr;
  }

  mStartRecordingPromise = promise;
  mOptions = aOptions;
  mRecording = true;
  return promise.forget();
}

void
nsDOMCameraControl::OnCreatedFileDescriptor(bool aSucceeded)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!mCameraControl) {
    rv = NS_ERROR_NOT_AVAILABLE;
  } else if (!mRecording) {
    // Race condition where StopRecording comes in before we issue
    // the start recording request to Gonk
    rv = NS_ERROR_ABORT;
    mOptions.mCreatePoster = false;
  } else if (aSucceeded && mDSFileDescriptor->mFileDescriptor.IsValid()) {
    ICameraControl::StartRecordingOptions o;

    o.rotation = mOptions.mRotation;
    o.maxFileSizeBytes = mOptions.mMaxFileSizeBytes;
    o.maxVideoLengthMs = mOptions.mMaxVideoLengthMs;
    o.autoEnableLowLightTorch = mOptions.mAutoEnableLowLightTorch;
    o.createPoster = mOptions.mCreatePoster;
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
    RefPtr<CloseFileRunnable> closer =
      new CloseFileRunnable(mDSFileDescriptor->mFileDescriptor);
    closer->Dispatch();
  }
}

void
nsDOMCameraControl::StopRecording(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();

  ReleaseAudioChannelAgent();
  mRecording = false;
  aRv = mCameraControl->StopRecording();
}

void
nsDOMCameraControl::PauseRecording(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();

  aRv = mCameraControl->PauseRecording();
}

void
nsDOMCameraControl::ResumeRecording(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();

  aRv = mCameraControl->ResumeRecording();
}

void
nsDOMCameraControl::ResumePreview(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->StartPreview();
}

already_AddRefed<Promise>
nsDOMCameraControl::SetConfiguration(const CameraConfiguration& aConfiguration,
                                     ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL(nullptr);

  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mTakePicturePromise) {
    // We're busy taking a picture, can't change modes right now.
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
    return promise.forget();
  }

  ICameraControl::Configuration config;
  aRv = SelectPreviewSize(aConfiguration.mPreviewSize, config.mPreviewSize);
  if (aRv.Failed()) {
    return nullptr;
  }

  config.mRecorderProfile = aConfiguration.mRecorderProfile;
  config.mPictureSize.width = aConfiguration.mPictureSize.mWidth;
  config.mPictureSize.height = aConfiguration.mPictureSize.mHeight;
  config.mMode = ICameraControl::kPictureMode;
  if (aConfiguration.mMode == CameraMode::Video) {
    config.mMode = ICameraControl::kVideoMode;
  }

  aRv = mCameraControl->SetConfiguration(config);
  if (aRv.Failed()) {
    return nullptr;
  }

  mSetConfigurationPromise = promise;
  return promise.forget();
}

already_AddRefed<Promise>
nsDOMCameraControl::AutoFocus(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL(nullptr);

  RefPtr<Promise> promise = mAutoFocusPromise.forget();
  if (promise) {
    // There is already a call to AutoFocus() in progress, cancel it and
    // invoke the error callback (if one was passed in).
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
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
  return promise.forget();
}

void
nsDOMCameraControl::StartFaceDetection(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->StartFaceDetection();
}

void
nsDOMCameraControl::StopFaceDetection(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->StopFaceDetection();
}

already_AddRefed<Promise>
nsDOMCameraControl::TakePicture(const CameraPictureOptions& aOptions,
                                ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL(nullptr);

  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mTakePicturePromise) {
    // There is already a call to TakePicture() in progress, abort this new
    // one and invoke the error callback (if one was passed in).
    promise->MaybeReject(NS_ERROR_IN_PROGRESS);
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
    if (!aOptions.mFileFormat.IsEmpty()) {
      mCameraControl->Set(CAMERA_PARAM_PICTURE_FILEFORMAT, aOptions.mFileFormat);
    }
    mCameraControl->Set(CAMERA_PARAM_PICTURE_ROTATION, aOptions.mRotation);
    mCameraControl->Set(CAMERA_PARAM_PICTURE_DATETIME, aOptions.mDateTime);
    mCameraControl->SetLocation(p);
  }

  aRv = mCameraControl->TakePicture();
  if (aRv.Failed()) {
    return nullptr;
  }

  mTakePicturePromise = promise;
  return promise.forget();
}

already_AddRefed<Promise>
nsDOMCameraControl::ReleaseHardware(ErrorResult& aRv)
{
  DOM_CAMERA_LOGI("%s:%d : this=%p\n", __func__, __LINE__, this);

  RefPtr<Promise> promise = CreatePromise(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mCameraControl) {
    // Always succeed if the camera instance is already closed.
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  aRv = mCameraControl->Stop();
  if (aRv.Failed()) {
    return nullptr;
  }

  // Once we stop the camera, there's nothing we can do with it,
  // so we can throw away this reference. (This won't prevent us
  // from receiving the last underlying events.)
  mCameraControl = nullptr;
  mReleasePromise = promise;

  return promise.forget();
}

void
nsDOMCameraControl::ResumeContinuousFocus(ErrorResult& aRv)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  THROW_IF_NO_CAMERACONTROL();
  aRv = mCameraControl->ResumeContinuousFocus();
}

void
nsDOMCameraControl::Shutdown()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  // Remove any pending solicited event handlers; these
  // reference our window object, which in turn references
  // us. If we don't remove them, we can leak DOM objects.
  AbortPromise(mGetCameraPromise);
  AbortPromise(mAutoFocusPromise);
  AbortPromise(mTakePicturePromise);
  AbortPromise(mStartRecordingPromise);
  AbortPromise(mReleasePromise);
  AbortPromise(mSetConfigurationPromise);

  if (mCameraControl) {
    mCameraControl->Stop();
    mCameraControl = nullptr;
  }
}

void
nsDOMCameraControl::ReleaseAudioChannelAgent()
{
#ifdef MOZ_B2G
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
  }
#endif
}

nsresult
nsDOMCameraControl::NotifyRecordingStatusChange(const nsString& aMsg)
{
  NS_ENSURE_TRUE(mWindow, NS_ERROR_FAILURE);

  if (aMsg.EqualsLiteral("shutdown")) {
    ReleaseAudioChannelAgent();
  }

  nsresult rv = MediaManager::NotifyRecordingStatusChange(mWindow,
                                                          aMsg,
                                                          true /* aIsAudio */,
                                                          true /* aIsVideo */);

  if (NS_FAILED(rv)) {
    return rv;
  }

#ifdef MOZ_B2G
  if (aMsg.EqualsLiteral("starting") && !mAudioChannelAgent) {
    mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
    if (!mAudioChannelAgent) {
      return NS_ERROR_UNEXPECTED;
    }

    // Camera app will stop recording when it falls to the background, so no callback is necessary.
    mAudioChannelAgent->Init(mWindow, (int32_t)AudioChannel::Content, nullptr);
    // Video recording doesn't output any sound, so it's not necessary to check canPlay.
    AudioPlaybackConfig config;
    rv = mAudioChannelAgent->NotifyStartedPlaying(&config, AudioChannelService::AudibleState::eAudible);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif
  return rv;
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
nsDOMCameraControl::AbortPromise(RefPtr<Promise>& aPromise)
{
  RefPtr<Promise> promise = aPromise.forget();
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

  RefPtr<CameraStateChangeEvent> event =
    CameraStateChangeEvent::Constructor(this, aType, eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnGetCameraComplete()
{
  // The hardware is open, so we can return a camera to JS, even if
  // the preview hasn't started yet.
  RefPtr<Promise> promise = mGetCameraPromise.forget();
  if (promise) {
    CameraGetPromiseData data;
    data.mCamera = this;
    data.mConfiguration = *mCurrentConfiguration;
    promise->MaybeResolve(data);
  }
}

// Camera Control event handlers--must only be called from the Main Thread!
void
nsDOMCameraControl::OnHardwareStateChange(CameraControlListener::HardwareState aState,
                                          nsresult aReason)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());

  switch (aState) {
    case CameraControlListener::kHardwareOpen:
      DOM_CAMERA_LOGI("DOM OnHardwareStateChange: open\n");
      MOZ_ASSERT(aReason == NS_OK);
      if (!mSetInitialConfig) {
        // The hardware is open, so we can return a camera to JS, even if
        // the preview hasn't started yet.
        OnGetCameraComplete();
      }
      break;

    case CameraControlListener::kHardwareClosed:
      DOM_CAMERA_LOGI("DOM OnHardwareStateChange: closed\n");
      if (!mSetInitialConfig) {
        RefPtr<Promise> promise = mReleasePromise.forget();
        if (promise) {
          promise->MaybeResolveWithUndefined();
        }

        CameraClosedEventInit eventInit;
        switch (aReason) {
          case NS_OK:
            eventInit.mReason = NS_LITERAL_STRING("HardwareReleased");
            break;

          case NS_ERROR_FAILURE:
            eventInit.mReason = NS_LITERAL_STRING("SystemFailure");
            break;

          case NS_ERROR_NOT_AVAILABLE:
            eventInit.mReason = NS_LITERAL_STRING("NotAvailable");
            break;

          default:
            DOM_CAMERA_LOGE("Unhandled hardware close reason, 0x%x\n", aReason);
            MOZ_ASSERT_UNREACHABLE("Unanticipated reason for hardware close");
            eventInit.mReason = NS_LITERAL_STRING("SystemFailure");
            break;
        }

        RefPtr<CameraClosedEvent> event =
          CameraClosedEvent::Constructor(this,
                                         NS_LITERAL_STRING("close"),
                                         eventInit);
        DispatchTrustedEvent(event);
      } else {
        // The configuration failed and we forced the camera to shutdown.
        OnUserError(DOMCameraControlListener::kInStartCamera, NS_ERROR_NOT_AVAILABLE);
      }
      break;

    case CameraControlListener::kHardwareOpenFailed:
      DOM_CAMERA_LOGI("DOM OnHardwareStateChange: open failed\n");
      MOZ_ASSERT(aReason == NS_ERROR_NOT_AVAILABLE);
      OnUserError(DOMCameraControlListener::kInStartCamera, NS_ERROR_NOT_AVAILABLE);
      break;

    case CameraControlListener::kHardwareUninitialized:
      break;

    default:
      DOM_CAMERA_LOGE("DOM OnHardwareStateChange: UNKNOWN=%d\n", aState);
      MOZ_ASSERT_UNREACHABLE("Unanticipated camera hardware state");
  }
}

void
nsDOMCameraControl::OnShutter()
{
  DOM_CAMERA_LOGI("DOM ** SNAP **\n");
  MOZ_ASSERT(NS_IsMainThread());
  DispatchTrustedEvent(NS_LITERAL_STRING("shutter"));
}

void
nsDOMCameraControl::OnPreviewStateChange(CameraControlListener::PreviewState aState)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
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

  DispatchPreviewStateEvent(aState);
}

void
nsDOMCameraControl::OnPoster(BlobImpl* aPoster)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOptions.mCreatePoster);

  RefPtr<Blob> blob = Blob::Create(GetParentObject(), aPoster);
  if (NS_WARN_IF(!blob)) {
    OnRecorderStateChange(CameraControlListener::kPosterFailed, 0, 0);
    return;
  }

  BlobEventInit eventInit;
  eventInit.mData = blob;

  RefPtr<BlobEvent> event = BlobEvent::Constructor(this,
                                                     NS_LITERAL_STRING("poster"),
                                                     eventInit);

  DispatchTrustedEvent(event);
  OnRecorderStateChange(CameraControlListener::kPosterCreated, 0, 0);
}

void
nsDOMCameraControl::OnRecorderStateChange(CameraControlListener::RecorderState aState,
                                          int32_t aArg, int32_t aTrackNum)
{
  // For now, we do nothing with 'aStatus' and 'aTrackNum'.
  DOM_CAMERA_LOGT("%s:%d : this=%p, state=%u\n", __func__, __LINE__, this, aState);
  MOZ_ASSERT(NS_IsMainThread());

  nsString state;

  switch (aState) {
    case CameraControlListener::kRecorderStarted:
      {
        RefPtr<Promise> promise = mStartRecordingPromise.forget();
        if (promise) {
          promise->MaybeResolveWithUndefined();
        }

        state = NS_LITERAL_STRING("Started");
      }
      break;

    case CameraControlListener::kRecorderStopped:
      if (mOptions.mCreatePoster) {
        mRecordingStoppedDeferred = true;
        return;
      }

      NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));
      state = NS_LITERAL_STRING("Stopped");
      break;

    case CameraControlListener::kPosterCreated:
      state = NS_LITERAL_STRING("PosterCreated");
      mOptions.mCreatePoster = false;
      break;

    case CameraControlListener::kPosterFailed:
      state = NS_LITERAL_STRING("PosterFailed");
      mOptions.mCreatePoster = false;
      break;

    case CameraControlListener::kRecorderPaused:
      state = NS_LITERAL_STRING("Paused");
      break;

    case CameraControlListener::kRecorderResumed:
      state = NS_LITERAL_STRING("Resumed");
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

  DispatchStateEvent(NS_LITERAL_STRING("recorderstatechange"), state);

  if (mRecordingStoppedDeferred && !mOptions.mCreatePoster) {
    mRecordingStoppedDeferred = false;
    OnRecorderStateChange(CameraControlListener::kRecorderStopped, 0, 0);
  }
}

void
nsDOMCameraControl::OnConfigurationChange(DOMCameraConfiguration* aConfiguration)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
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
  DOM_CAMERA_LOGI("    picture size (w x h)   : %d x %d\n",
    mCurrentConfiguration->mPictureSize.mWidth, mCurrentConfiguration->mPictureSize.mHeight);
  DOM_CAMERA_LOGI("    recorder profile       : %s\n",
    NS_ConvertUTF16toUTF8(mCurrentConfiguration->mRecorderProfile).get());

  if (mSetInitialConfig) {
    OnGetCameraComplete();
    mSetInitialConfig = false;
    return;
  }

  RefPtr<Promise> promise = mSetConfigurationPromise.forget();
  if (promise) {
    promise->MaybeResolve(*aConfiguration);
  }

  CameraConfigurationEventInit eventInit;
  eventInit.mMode = mCurrentConfiguration->mMode;
  eventInit.mRecorderProfile = mCurrentConfiguration->mRecorderProfile;
  eventInit.mPreviewSize = new DOMRect(static_cast<DOMMediaStream*>(this), 0, 0,
                                       mCurrentConfiguration->mPreviewSize.mWidth,
                                       mCurrentConfiguration->mPreviewSize.mHeight);
  eventInit.mPictureSize = new DOMRect(static_cast<DOMMediaStream*>(this), 0, 0,
                                       mCurrentConfiguration->mPictureSize.mWidth,
                                       mCurrentConfiguration->mPictureSize.mHeight);

  RefPtr<CameraConfigurationEvent> event =
    CameraConfigurationEvent::Constructor(this,
                                          NS_LITERAL_STRING("configurationchanged"),
                                          eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnAutoFocusComplete(bool aAutoFocusSucceeded)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Promise> promise = mAutoFocusPromise.forget();
  if (promise) {
    promise->MaybeResolve(aAutoFocusSucceeded);
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
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());

  if (aIsMoving) {
    DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("focusing"));
  }
}

void
nsDOMCameraControl::OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces)
{
  DOM_CAMERA_LOGI("DOM OnFacesDetected %zu face(s)\n", aFaces.Length());
  MOZ_ASSERT(NS_IsMainThread());

  Sequence<OwningNonNull<DOMCameraDetectedFace> > faces;
  uint32_t len = aFaces.Length();

  if (faces.SetCapacity(len, fallible)) {
    for (uint32_t i = 0; i < len; ++i) {
      *faces.AppendElement(fallible) =
        new DOMCameraDetectedFace(static_cast<DOMMediaStream*>(this), aFaces[i]);
    }
  }

  CameraFacesDetectedEventInit eventInit;
  eventInit.mFaces.SetValue(faces);

  RefPtr<CameraFacesDetectedEvent> event =
    CameraFacesDetectedEvent::Constructor(this,
                                          NS_LITERAL_STRING("facesdetected"),
                                          eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnTakePictureComplete(nsIDOMBlob* aPicture)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPicture);

  RefPtr<Promise> promise = mTakePicturePromise.forget();
  if (promise) {
    nsCOMPtr<nsIDOMBlob> picture = aPicture;
    promise->MaybeResolve(picture);
  }

  RefPtr<Blob> blob = static_cast<Blob*>(aPicture);
  BlobEventInit eventInit;
  eventInit.mData = blob;

  RefPtr<BlobEvent> event = BlobEvent::Constructor(this,
                                                     NS_LITERAL_STRING("picture"),
                                                     eventInit);

  DispatchTrustedEvent(event);
}

void
nsDOMCameraControl::OnUserError(CameraControlListener::UserContext aContext, nsresult aError)
{
  DOM_CAMERA_LOGI("DOM OnUserError : this=%p, aContext=%u, aError=0x%x\n",
    this, aContext, aError);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Promise> promise;

  switch (aContext) {
    case CameraControlListener::kInStartCamera:
      promise = mGetCameraPromise.forget();
      // If we failed to open the camera, we never actually provided a reference
      // for the application to release explicitly. Thus we must clear our handle
      // here to ensure everything is freed.
      mCameraControl = nullptr;
      break;

    case CameraControlListener::kInStopCamera:
      promise = mReleasePromise.forget();
      if (aError == NS_ERROR_NOT_INITIALIZED) {
        // This value indicates that the hardware is already closed; which for
        // kInStopCamera, is not actually an error.
        if (promise) {
          promise->MaybeResolveWithUndefined();
        }

        return;
      }
      break;

    case CameraControlListener::kInSetConfiguration:
      if (mSetInitialConfig && mCameraControl) {
        // If the SetConfiguration() call in the constructor fails, there
        // is nothing we can do except release the camera hardware. This
        // will trigger a hardware state change, and when the flag that
        // got us here is set in that handler, we replace the normal reason
        // code with one that indicates the hardware isn't available.
        DOM_CAMERA_LOGI("Failed to configure cached camera, stopping\n");
        mCameraControl->Stop();
        return;
      }
      promise = mSetConfigurationPromise.forget();
      break;

    case CameraControlListener::kInAutoFocus:
      promise = mAutoFocusPromise.forget();
      DispatchStateEvent(NS_LITERAL_STRING("focus"), NS_LITERAL_STRING("unfocused"));
      break;

    case CameraControlListener::kInTakePicture:
      promise = mTakePicturePromise.forget();
      break;

    case CameraControlListener::kInStartRecording:
      promise = mStartRecordingPromise.forget();
      mRecording = false;
      NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));
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

    case CameraControlListener::kInPauseRecording:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to pause recording");
      return;

    case CameraControlListener::kInResumeRecording:
      // This method doesn't have any callbacks, so all we can do is log the
      // failure. This only happens after the hardware has been released.
      NS_WARNING("Failed to resume recording");
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

  if (!promise) {
    DOM_CAMERA_LOGW("DOM No error handler for aError=0x%x in aContext=%u\n",
      aError, aContext);
    return;
  }

  promise->MaybeReject(aError);
}
