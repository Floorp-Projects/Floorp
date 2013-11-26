/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "MediaStreamGraph.h"
#include "GetUserMediaRequest.h"
#include "nsHashPropertyBag.h"
#ifdef MOZ_WIDGET_GONK
#include "nsIAudioManager.h"
#endif
#include "nsIDOMFile.h"
#include "nsIEventTarget.h"
#include "nsIUUIDGenerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPopupWindowManager.h"
#include "nsISupportsArray.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"

#include "Latency.h"

// For PR_snprintf
#include "prprf.h"

#include "nsJSUtils.h"
#include "nsDOMFile.h"
#include "nsGlobalWindow.h"

#include "mozilla/Preferences.h"

/* Using WebRTC backend on Desktops (Mac, Windows, Linux), otherwise default */
#include "MediaEngineDefault.h"
#if defined(MOZ_WEBRTC)
#include "MediaEngineWebRTC.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "MediaPermissionGonk.h"
#endif

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaStream::GetCurrentTime.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
PRLogModuleInfo*
GetMediaManagerLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("MediaManager");
  return sLog;
}
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#else
#define LOG(msg)
#endif

using dom::MediaStreamConstraints;         // Outside API (contains JSObject)
using dom::MediaStreamConstraintsInternal; // Storable supported constraints
using dom::MediaTrackConstraintsInternal;  // Video or audio constraints
using dom::MediaTrackConstraintSet;        // Mandatory or optional constraints
using dom::MediaTrackConstraints;          // Raw mMandatory (as JSObject)
using dom::GetUserMediaRequest;
using dom::Sequence;

// Used to compare raw MediaTrackConstraintSet against normalized dictionary
// version to detect member differences, e.g. unsupported constraints.

static nsresult CompareDictionaries(JSContext* aCx, JSObject *aA,
                                    const MediaTrackConstraintSet &aB,
                                    nsString *aDifference)
{
  JS::Rooted<JSObject*> a(aCx, aA);
  JSAutoCompartment ac(aCx, aA);
  JS::Rooted<JS::Value> bval(aCx);
  aB.ToObject(aCx, JS::NullPtr(), &bval);
  JS::Rooted<JSObject*> b(aCx, &bval.toObject());

  // Iterate over each property in A, and check if it is in B

  JS::AutoIdArray props(aCx, JS_Enumerate(aCx, a));

  for (size_t i = 0; i < props.length(); i++) {
    JS::Rooted<JS::Value> bprop(aCx);
    if (!JS_GetPropertyById(aCx, b, props[i], &bprop)) {
      LOG(("Error parsing dictionary!\n"));
      return NS_ERROR_UNEXPECTED;
    }
    if (bprop.isUndefined()) {
      // Unknown property found in A. Bail with name
      JS::Rooted<JS::Value> nameval(aCx);
      bool success = JS_IdToValue(aCx, props[i], nameval.address());
      NS_ENSURE_TRUE(success, NS_ERROR_UNEXPECTED);

      JS::Rooted<JSString*> namestr(aCx, JS::ToString(aCx, nameval));
      NS_ENSURE_TRUE(namestr, NS_ERROR_UNEXPECTED);
      aDifference->Assign(JS_GetStringCharsZ(aCx, namestr));
      return NS_OK;
    }
  }
  aDifference->Truncate();
  return NS_OK;
}

// Look for and return any unknown mandatory constraint. Done by comparing
// a raw MediaTrackConstraints against a normalized copy, both passed in.

static nsresult ValidateTrackConstraints(
    JSContext *aCx, JSObject *aRaw,
    const MediaTrackConstraintsInternal &aNormalized,
    nsString *aOutUnknownConstraint)
{
  // First find raw mMandatory member (use MediaTrackConstraints as helper)
  JS::Rooted<JS::Value> rawval(aCx, JS::ObjectValue(*aRaw));
  dom::RootedDictionary<MediaTrackConstraints> track(aCx);
  bool success = track.Init(aCx, rawval);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  if (track.mMandatory.WasPassed()) {
    nsresult rv = CompareDictionaries(aCx, track.mMandatory.Value(),
                                      aNormalized.mMandatory,
                                      aOutUnknownConstraint);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/**
 * Send an error back to content. The error is the form a string.
 * Do this only on the main thread. The success callback is also passed here
 * so it can be released correctly.
 */
class ErrorCallbackRunnable : public nsRunnable
{
public:
  ErrorCallbackRunnable(
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    const nsAString& aErrorMsg, uint64_t aWindowID)
    : mSuccess(aSuccess)
    , mError(aError)
    , mErrorMsg(aErrorMsg)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance()) {}

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success(mSuccess);
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error(mError);

    if (!(mManager->IsWindowStillActive(mWindowID))) {
      return NS_OK;
    }
    // This is safe since we're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    error->OnError(mErrorMsg);
    return NS_OK;
  }

private:
  already_AddRefed<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  already_AddRefed<nsIDOMGetUserMediaErrorCallback> mError;
  const nsString mErrorMsg;
  uint64_t mWindowID;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * Invoke the "onSuccess" callback in content. The callback will take a
 * DOMBlob in the case of {picture:true}, and a MediaStream in the case of
 * {audio:true} or {video:true}. There is a constructor available for each
 * form. Do this only on the main thread.
 */
class SuccessCallbackRunnable : public nsRunnable
{
public:
  SuccessCallbackRunnable(
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    nsIDOMFile* aFile, uint64_t aWindowID)
    : mSuccess(aSuccess)
    , mError(aError)
    , mFile(aFile)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance()) {}

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success(mSuccess);
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error(mError);

    if (!(mManager->IsWindowStillActive(mWindowID))) {
      return NS_OK;
    }
    // This is safe since we're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    success->OnSuccess(mFile);
    return NS_OK;
  }

private:
  already_AddRefed<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  already_AddRefed<nsIDOMGetUserMediaErrorCallback> mError;
  nsCOMPtr<nsIDOMFile> mFile;
  uint64_t mWindowID;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * Invoke the GetUserMediaDevices success callback. Wrapped in a runnable
 * so that it may be called on the main thread. The error callback is also
 * passed so it can be released correctly.
 */
class DeviceSuccessCallbackRunnable: public nsRunnable
{
public:
  DeviceSuccessCallbackRunnable(
    already_AddRefed<nsIGetUserMediaDevicesSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    nsTArray<nsCOMPtr<nsIMediaDevice> >* aDevices)
    : mSuccess(aSuccess)
    , mError(aError)
    , mDevices(aDevices) {}

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> success(mSuccess);
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error(mError);

    nsCOMPtr<nsIWritableVariant> devices =
      do_CreateInstance("@mozilla.org/variant;1");

    int32_t len = mDevices->Length();
    if (len == 0) {
      // XXX
      // We should in the future return an empty array, and dynamically add
      // devices to the dropdowns if things are hotplugged while the
      // requester is up.
      error->OnError(NS_LITERAL_STRING("NO_DEVICES_FOUND"));
      return NS_OK;
    }

    nsTArray<nsIMediaDevice*> tmp(len);
    for (int32_t i = 0; i < len; i++) {
      tmp.AppendElement(mDevices->ElementAt(i));
    }

    devices->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                        &NS_GET_IID(nsIMediaDevice),
                        mDevices->Length(),
                        const_cast<void*>(
                          static_cast<const void*>(tmp.Elements())
                        ));

    success->OnSuccess(devices);
    return NS_OK;
  }

private:
  already_AddRefed<nsIGetUserMediaDevicesSuccessCallback> mSuccess;
  already_AddRefed<nsIDOMGetUserMediaErrorCallback> mError;
  nsAutoPtr<nsTArray<nsCOMPtr<nsIMediaDevice> > > mDevices;
};

// Handle removing GetUserMediaCallbackMediaStreamListener from main thread
class GetUserMediaListenerRemove: public nsRunnable
{
public:
  GetUserMediaListenerRemove(uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener *aListener)
    : mWindowID(aWindowID)
    , mListener(aListener) {}

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    nsRefPtr<MediaManager> manager(MediaManager::GetInstance());
    manager->RemoveFromWindowList(mWindowID, mListener);
    return NS_OK;
  }

protected:
  uint64_t mWindowID;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
};

/**
 * nsIMediaDevice implementation.
 */
NS_IMPL_ISUPPORTS1(MediaDevice, nsIMediaDevice)

MediaDevice::MediaDevice(MediaEngineVideoSource* aSource)
  : mHasFacingMode(false)
  , mSource(aSource) {
  mType.Assign(NS_LITERAL_STRING("video"));
  mSource->GetName(mName);
  mSource->GetUUID(mID);

#ifdef MOZ_B2G_CAMERA
  if (mName.EqualsLiteral("back")) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::Environment;
  } else if (mName.EqualsLiteral("front")) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::User;
  }
#endif // MOZ_B2G_CAMERA

  // Kludge to test user-facing cameras on OSX.
  if (mName.Find(NS_LITERAL_STRING("Face")) != -1) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::User;
  }
}

MediaDevice::MediaDevice(MediaEngineAudioSource* aSource)
  : mHasFacingMode(false)
  , mSource(aSource) {
  mType.Assign(NS_LITERAL_STRING("audio"));
  mSource->GetName(mName);
  mSource->GetUUID(mID);
}

NS_IMETHODIMP
MediaDevice::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetType(nsAString& aType)
{
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetId(nsAString& aID)
{
  aID.Assign(mID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetFacingMode(nsAString& aFacingMode)
{
  if (mHasFacingMode) {
    aFacingMode.Assign(NS_ConvertUTF8toUTF16(
        dom::VideoFacingModeEnumValues::strings[uint32_t(mFacingMode)].value));
  } else {
    aFacingMode.Truncate(0);
  }
  return NS_OK;
}

MediaEngineSource*
MediaDevice::GetSource()
{
  return mSource;
}

/**
 * A subclass that we only use to stash internal pointers to MediaStreamGraph objects
 * that need to be cleaned up.
 */
class nsDOMUserMediaStream : public DOMLocalMediaStream
{
public:
  static already_AddRefed<nsDOMUserMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow, uint32_t aHintContents)
  {
    nsRefPtr<nsDOMUserMediaStream> stream = new nsDOMUserMediaStream();
    stream->InitTrackUnionStream(aWindow, aHintContents);
    return stream.forget();
  }

  virtual ~nsDOMUserMediaStream()
  {
    Stop();

    if (mPort) {
      mPort->Destroy();
    }
    if (mSourceStream) {
      mSourceStream->Destroy();
    }
  }

  virtual void Stop()
  {
    if (mSourceStream) {
      mSourceStream->EndAllTrackAndFinish();
    }
  }

  // Allow getUserMedia to pass input data directly to PeerConnection/MediaPipeline
  virtual bool AddDirectListener(MediaStreamDirectListener *aListener) MOZ_OVERRIDE
  {
    if (mSourceStream) {
      mSourceStream->AddDirectListener(aListener);
      return true; // application should ignore NotifyQueuedTrackData
    }
    return false;
  }

  virtual void RemoveDirectListener(MediaStreamDirectListener *aListener) MOZ_OVERRIDE
  {
    if (mSourceStream) {
      mSourceStream->RemoveDirectListener(aListener);
    }
  }

  // let us intervene for direct listeners when someone does track.enabled = false
  virtual void SetTrackEnabled(TrackID aID, bool aEnabled) MOZ_OVERRIDE
  {
    // We encapsulate the SourceMediaStream and TrackUnion into one entity, so
    // we can handle the disabling at the SourceMediaStream

    // We need to find the input track ID for output ID aID, so we let the TrackUnion
    // forward the request to the source and translate the ID
    GetStream()->AsProcessedStream()->ForwardTrackEnabled(aID, aEnabled);
  }

  // The actual MediaStream is a TrackUnionStream. But these resources need to be
  // explicitly destroyed too.
  nsRefPtr<SourceMediaStream> mSourceStream;
  nsRefPtr<MediaInputPort> mPort;
};

/**
 * Creates a MediaStream, attaches a listener and fires off a success callback
 * to the DOM with the stream. We also pass in the error callback so it can
 * be released correctly.
 *
 * All of this must be done on the main thread!
 *
 * Note that the various GetUserMedia Runnable classes currently allow for
 * two streams.  If we ever need to support getting more than two streams
 * at once, we could convert everything to nsTArray<nsRefPtr<blah> >'s,
 * though that would complicate the constructors some.  Currently the
 * GetUserMedia spec does not allow for more than 2 streams to be obtained in
 * one call, to simplify handling of constraints.
 */
class GetUserMediaStreamRunnable : public nsRunnable
{
public:
  GetUserMediaStreamRunnable(
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener* aListener,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource)
    : mSuccess(aSuccess)
    , mError(aError)
    , mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mWindowID(aWindowID)
    , mListener(aListener)
    , mManager(MediaManager::GetInstance()) {}

  ~GetUserMediaStreamRunnable() {}

  class TracksAvailableCallback : public DOMMediaStream::OnTracksAvailableCallback
  {
  public:
    TracksAvailableCallback(MediaManager* aManager,
                            nsIDOMGetUserMediaSuccessCallback* aSuccess,
                            uint64_t aWindowID,
                            DOMMediaStream* aStream)
      : mWindowID(aWindowID), mSuccess(aSuccess), mManager(aManager),
        mStream(aStream) {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream) MOZ_OVERRIDE
    {
      // We're in the main thread, so no worries here.
      if (!(mManager->IsWindowStillActive(mWindowID))) {
        return;
      }

      // Start currentTime from the point where this stream was successfully
      // returned.
      aStream->SetLogicalStreamStartTime(aStream->GetStream()->GetCurrentTime());

      // This is safe since we're on main-thread, and the windowlist can only
      // be invalidated from the main-thread (see OnNavigation)
      LOG(("Returning success for getUserMedia()"));
      mSuccess->OnSuccess(aStream);
    }
    uint64_t mWindowID;
    nsRefPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
    nsRefPtr<MediaManager> mManager;
    // Keep the DOMMediaStream alive until the NotifyTracksAvailable callback
    // has fired, otherwise we might immediately destroy the DOMMediaStream and
    // shut down the underlying MediaStream prematurely.
    // This creates a cycle which is broken when NotifyTracksAvailable
    // is fired (which will happen unless the browser shuts down,
    // since we only add this callback when we've successfully appended
    // the desired tracks in the MediaStreamGraph) or when
    // DOMMediaStream::NotifyMediaStreamGraphShutdown is called.
    nsRefPtr<DOMMediaStream> mStream;
  };

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(mWindowID));

    // We're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    StreamListeners* listeners = mManager->GetWindowListeners(mWindowID);
    if (!listeners || !window || !window->GetExtantDoc()) {
      // This window is no longer live.  mListener has already been removed
      return NS_OK;
    }

    // Create a media stream.
    DOMMediaStream::TrackTypeHints hints =
      (mAudioSource ? DOMMediaStream::HINT_CONTENTS_AUDIO : 0) |
      (mVideoSource ? DOMMediaStream::HINT_CONTENTS_VIDEO : 0);

    nsRefPtr<nsDOMUserMediaStream> trackunion =
      nsDOMUserMediaStream::CreateTrackUnionStream(window, hints);
    if (!trackunion) {
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error = mError.forget();
      LOG(("Returning error for getUserMedia() - no stream"));
      error->OnError(NS_LITERAL_STRING("NO_STREAM"));
      return NS_OK;
    }

    MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
    nsRefPtr<SourceMediaStream> stream = gm->CreateSourceStream(nullptr);

    // connect the source stream to the track union stream to avoid us blocking
    trackunion->GetStream()->AsProcessedStream()->SetAutofinish(true);
    nsRefPtr<MediaInputPort> port = trackunion->GetStream()->AsProcessedStream()->
      AllocateInputPort(stream, MediaInputPort::FLAG_BLOCK_OUTPUT);
    trackunion->mSourceStream = stream;
    trackunion->mPort = port.forget();
    // Log the relationship between SourceMediaStream and TrackUnion stream
    // Make sure logger starts before capture
    AsyncLatencyLogger::Get(true);
    LogLatency(AsyncLatencyLogger::MediaStreamCreate,
               reinterpret_cast<uint64_t>(stream.get()),
               reinterpret_cast<int64_t>(trackunion->GetStream()));

    trackunion->CombineWithPrincipal(window->GetExtantDoc()->NodePrincipal());

    // The listener was added at the begining in an inactive state.
    // Activate our listener. We'll call Start() on the source when get a callback
    // that the MediaStream has started consuming. The listener is freed
    // when the page is invalidated (on navigation or close).
    mListener->Activate(stream.forget(), mAudioSource, mVideoSource);

    TracksAvailableCallback* tracksAvailableCallback =
      new TracksAvailableCallback(mManager, mSuccess, mWindowID, trackunion);

    // Dispatch to the media thread to ask it to start the sources,
    // because that can take a while.
    // Pass ownership of trackunion to the MediaOperationRunnable
    // to ensure it's kept alive until the MediaOperationRunnable runs (at least).
    nsIThread *mediaThread = MediaManager::GetThread();
    nsRefPtr<MediaOperationRunnable> runnable(
      new MediaOperationRunnable(MEDIA_START, mListener, trackunion,
                                 tracksAvailableCallback,
                                 mAudioSource, mVideoSource, false, mWindowID));
    mediaThread->Dispatch(runnable, NS_DISPATCH_NORMAL);

#ifdef MOZ_WEBRTC
    // Right now these configs are only of use if webrtc is available
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

      if (branch) {
        int32_t aec = (int32_t) webrtc::kEcUnchanged;
        int32_t agc = (int32_t) webrtc::kAgcUnchanged;
        int32_t noise = (int32_t) webrtc::kNsUnchanged;
        bool aec_on = false, agc_on = false, noise_on = false;

        branch->GetBoolPref("media.peerconnection.aec_enabled", &aec_on);
        branch->GetIntPref("media.peerconnection.aec", &aec);
        branch->GetBoolPref("media.peerconnection.agc_enabled", &agc_on);
        branch->GetIntPref("media.peerconnection.agc", &agc);
        branch->GetBoolPref("media.peerconnection.noise_enabled", &noise_on);
        branch->GetIntPref("media.peerconnection.noise", &noise);

        mListener->AudioConfig(aec_on, (uint32_t) aec,
                               agc_on, (uint32_t) agc,
                               noise_on, (uint32_t) noise);
      }
    }
#endif

    // We won't need mError now.
    mError = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsRefPtr<nsIDOMGetUserMediaErrorCallback> mError;
  nsRefPtr<MediaEngineSource> mAudioSource;
  nsRefPtr<MediaEngineSource> mVideoSource;
  uint64_t mWindowID;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * Helper functions that implement the constraints algorithm from
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#methods-5
 */

static bool SatisfyConstraint(const MediaEngineVideoSource *,
                              const MediaTrackConstraintSet &aConstraints,
                              nsIMediaDevice &aCandidate)
{
  if (aConstraints.mFacingMode.WasPassed()) {
    nsString s;
    aCandidate.GetFacingMode(s);
    if (!s.EqualsASCII(dom::VideoFacingModeEnumValues::strings[
        uint32_t(aConstraints.mFacingMode.Value())].value)) {
      return false;
    }
  }
  // TODO: Add more video-specific constraints
  return true;
}

static bool SatisfyConstraint(const MediaEngineAudioSource *,
                              const MediaTrackConstraintSet &aConstraints,
                              nsIMediaDevice &aCandidate)
{
  // TODO: Add audio-specific constraints
  return true;
}

typedef nsTArray<nsCOMPtr<nsIMediaDevice> > SourceSet;

// Source getter that constrains list returned

template<class SourceType>
static SourceSet *
  GetSources(MediaEngine *engine,
             const MediaTrackConstraintsInternal &aConstraints,
             void (MediaEngine::* aEnumerate)(nsTArray<nsRefPtr<SourceType> >*))
{
  const SourceType * const type = nullptr;

  // First collect sources
  SourceSet candidateSet;
  {
    nsTArray<nsRefPtr<SourceType> > sources;
    (engine->*aEnumerate)(&sources);

    /**
      * We're allowing multiple tabs to access the same camera for parity
      * with Chrome.  See bug 811757 for some of the issues surrounding
      * this decision.  To disallow, we'd filter by IsAvailable() as we used
      * to.
      */

    for (uint32_t len = sources.Length(), i = 0; i < len; i++) {
      candidateSet.AppendElement(new MediaDevice(sources[i]));
    }
  }

  // Then apply mandatory constraints

  // Note: Iterator must be signed as it can dip below zero
  for (int i = 0; i < int(candidateSet.Length()); i++) {
    // Overloading instead of template specialization keeps things local
    if (!SatisfyConstraint(type, aConstraints.mMandatory, *candidateSet[i])) {
      candidateSet.RemoveElementAt(i--);
    }
  }

  // Then apply optional constraints.
  //
  // These are only effective when there are multiple sources to pick from.
  // Spec as-of-this-writing says to run algorithm on "all possible tracks
  // of media type T that the browser COULD RETURN" (emphasis added).
  //
  // We think users ultimately control which devices we could return, so after
  // determining the webpage's preferred list, we add the remaining choices
  // to the tail, reasoning that they would all have passed individually,
  // i.e. if the user had any one of them as their sole device (enabled).
  //
  // This avoids users having to unplug/disable devices should a webpage pick
  // the wrong one (UX-fail). Webpage-preferred devices will be listed first.

  SourceSet tailSet;

  if (aConstraints.mOptional.WasPassed()) {
    const Sequence<MediaTrackConstraintSet> &array = aConstraints.mOptional.Value();
    for (int i = 0; i < int(array.Length()); i++) {
      SourceSet rejects;
      // Note: Iterator must be signed as it can dip below zero
      for (int j = 0; j < int(candidateSet.Length()); j++) {
        if (!SatisfyConstraint(type, array[i], *candidateSet[j])) {
          rejects.AppendElement(candidateSet[j]);
          candidateSet.RemoveElementAt(j--);
        }
      }
      (candidateSet.Length()? tailSet : candidateSet).MoveElementsFrom(rejects);
    }
  }

  SourceSet *result = new SourceSet;
  result->MoveElementsFrom(candidateSet);
  result->MoveElementsFrom(tailSet);
  return result;
}

/**
 * Runs on a seperate thread and is responsible for enumerating devices.
 * Depending on whether a picture or stream was asked for, either
 * ProcessGetUserMedia or ProcessGetUserMediaSnapshot is called, and the results
 * are sent back to the DOM.
 *
 * Do not run this on the main thread. The success and error callbacks *MUST*
 * be dispatched on the main thread!
 */
class GetUserMediaRunnable : public nsRunnable
{
public:
  GetUserMediaRunnable(
    const MediaStreamConstraintsInternal& aConstraints,
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    uint64_t aWindowID, GetUserMediaCallbackMediaStreamListener *aListener,
    MediaEnginePrefs &aPrefs)
    : mConstraints(aConstraints)
    , mSuccess(aSuccess)
    , mError(aError)
    , mWindowID(aWindowID)
    , mListener(aListener)
    , mPrefs(aPrefs)
    , mDeviceChosen(false)
    , mBackendChosen(false)
    , mManager(MediaManager::GetInstance())
  {}

  /**
   * The caller can also choose to provide their own backend instead of
   * using the one provided by MediaManager::GetBackend.
   */
  GetUserMediaRunnable(
    const MediaStreamConstraintsInternal& aConstraints,
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    uint64_t aWindowID, GetUserMediaCallbackMediaStreamListener *aListener,
    MediaEnginePrefs &aPrefs,
    MediaEngine* aBackend)
    : mConstraints(aConstraints)
    , mSuccess(aSuccess)
    , mError(aError)
    , mWindowID(aWindowID)
    , mListener(aListener)
    , mPrefs(aPrefs)
    , mDeviceChosen(false)
    , mBackendChosen(true)
    , mBackend(aBackend)
    , mManager(MediaManager::GetInstance())
  {}

  ~GetUserMediaRunnable() {
    if (mBackendChosen) {
      delete mBackend;
    }
  }

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

    // Was a backend provided?
    if (!mBackendChosen) {
      mBackend = mManager->GetBackend(mWindowID);
    }

    // Was a device provided?
    if (!mDeviceChosen) {
      nsresult rv = SelectDevice();
      if (rv != NS_OK) {
        return rv;
      }
    }

    // It is an error if audio or video are requested along with picture.
    if (mConstraints.mPicture && (mConstraints.mAudio || mConstraints.mVideo)) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mSuccess, mError, NS_LITERAL_STRING("NOT_SUPPORTED_ERR"), mWindowID
      ));
      return NS_OK;
    }

    if (mConstraints.mPicture) {
      ProcessGetUserMediaSnapshot(mVideoDevice->GetSource(), 0);
      return NS_OK;
    }

    // There's a bug in the permission code that can leave us with mAudio but no audio device
    ProcessGetUserMedia(((mConstraints.mAudio && mAudioDevice) ?
                         mAudioDevice->GetSource() : nullptr),
                        ((mConstraints.mVideo && mVideoDevice) ?
                         mVideoDevice->GetSource() : nullptr));
    return NS_OK;
  }

  nsresult
  Denied(const nsAString& aErrorMsg)
  {
      // We add a disabled listener to the StreamListeners array until accepted
      // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      // This is safe since we're on main-thread, and the window can only
      // be invalidated from the main-thread (see OnNavigation)
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error(mError);
      error->OnError(aErrorMsg);

      // Should happen *after* error runs for consistency, but may not matter
      nsRefPtr<MediaManager> manager(MediaManager::GetInstance());
      manager->RemoveFromWindowList(mWindowID, mListener);
    } else {
      // This will re-check the window being alive on main-thread
      // Note: we must remove the listener on MainThread as well
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mSuccess, mError, aErrorMsg, mWindowID
      ));

      // MUST happen after ErrorCallbackRunnable Run()s, as it checks the active window list
      NS_DispatchToMainThread(new GetUserMediaListenerRemove(mWindowID, mListener));
    }

    return NS_OK;
  }

  nsresult
  SetAudioDevice(MediaDevice* aAudioDevice)
  {
    mAudioDevice = aAudioDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SetVideoDevice(MediaDevice* aVideoDevice)
  {
    mVideoDevice = aVideoDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SelectDevice()
  {
    if (mConstraints.mPicture || mConstraints.mVideo) {
      ScopedDeletePtr<SourceSet> sources (GetSources(mBackend,
          mConstraints.mVideom, &MediaEngine::EnumerateVideoDevices));

      if (!sources->Length()) {
        NS_DispatchToMainThread(new ErrorCallbackRunnable(
          mSuccess, mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID));
        return NS_ERROR_FAILURE;
      }
      // Pick the first available device.
      mVideoDevice = do_QueryObject((*sources)[0]);
      LOG(("Selected video device"));
    }

    if (mConstraints.mAudio) {
      ScopedDeletePtr<SourceSet> sources (GetSources(mBackend,
          mConstraints.mAudiom, &MediaEngine::EnumerateAudioDevices));

      if (!sources->Length()) {
        NS_DispatchToMainThread(new ErrorCallbackRunnable(
          mSuccess, mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID));
        return NS_ERROR_FAILURE;
      }
      // Pick the first available device.
      mAudioDevice = do_QueryObject((*sources)[0]);
      LOG(("Selected audio device"));
    }

    return NS_OK;
  }

  /**
   * Allocates a video or audio device and returns a MediaStream via
   * a GetUserMediaStreamRunnable. Runs off the main thread.
   */
  void
  ProcessGetUserMedia(MediaEngineSource* aAudioSource, MediaEngineSource* aVideoSource)
  {
    nsresult rv;
    if (aAudioSource) {
      rv = aAudioSource->Allocate(mPrefs);
      if (NS_FAILED(rv)) {
        LOG(("Failed to allocate audiosource %d",rv));
        NS_DispatchToMainThread(new ErrorCallbackRunnable(
                                  mSuccess, mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
                                                          ));
        return;
      }
    }
    if (aVideoSource) {
      rv = aVideoSource->Allocate(mPrefs);
      if (NS_FAILED(rv)) {
        LOG(("Failed to allocate videosource %d\n",rv));
        if (aAudioSource) {
          aAudioSource->Deallocate();
        }
        NS_DispatchToMainThread(new ErrorCallbackRunnable(
          mSuccess, mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
                                                          ));
        return;
      }
    }

    NS_DispatchToMainThread(new GetUserMediaStreamRunnable(
      mSuccess, mError, mWindowID, mListener, aAudioSource, aVideoSource
    ));
    return;
  }

  /**
   * Allocates a video device, takes a snapshot and returns a DOMFile via
   * a SuccessRunnable or an error via the ErrorRunnable. Off the main thread.
   */
  void
  ProcessGetUserMediaSnapshot(MediaEngineSource* aSource, int aDuration)
  {
    nsresult rv = aSource->Allocate(mPrefs);
    if (NS_FAILED(rv)) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mSuccess, mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
      ));
      return;
    }

    /**
     * Display picture capture UI here before calling Snapshot() - Bug 748835.
     */
    nsCOMPtr<nsIDOMFile> file;
    aSource->Snapshot(aDuration, getter_AddRefs(file));
    aSource->Deallocate();

    NS_DispatchToMainThread(new SuccessCallbackRunnable(
      mSuccess, mError, file, mWindowID
    ));
    return;
  }

private:
  MediaStreamConstraintsInternal mConstraints;

  already_AddRefed<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  already_AddRefed<nsIDOMGetUserMediaErrorCallback> mError;
  uint64_t mWindowID;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
  nsRefPtr<MediaDevice> mAudioDevice;
  nsRefPtr<MediaDevice> mVideoDevice;
  MediaEnginePrefs mPrefs;

  bool mDeviceChosen;
  bool mBackendChosen;

  MediaEngine* mBackend;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * Similar to GetUserMediaRunnable, but used for the chrome-only
 * GetUserMediaDevices function. Enumerates a list of audio & video devices,
 * wraps them up in nsIMediaDevice objects and returns it to the success
 * callback.
 */
class GetUserMediaDevicesRunnable : public nsRunnable
{
public:
  GetUserMediaDevicesRunnable(
    const MediaStreamConstraintsInternal& aConstraints,
    already_AddRefed<nsIGetUserMediaDevicesSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    uint64_t aWindowId)
    : mConstraints(aConstraints)
    , mSuccess(aSuccess)
    , mError(aError)
    , mManager(MediaManager::GetInstance())
    , mWindowId(aWindowId) {}

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");
    MediaEngine *backend = mManager->GetBackend(mWindowId);

    ScopedDeletePtr<SourceSet> final (GetSources(backend, mConstraints.mVideom,
                                          &MediaEngine::EnumerateVideoDevices));
    {
      ScopedDeletePtr<SourceSet> s (GetSources(backend, mConstraints.mAudiom,
                                        &MediaEngine::EnumerateAudioDevices));
      final->MoveElementsFrom(*s);
    }
    NS_DispatchToMainThread(new DeviceSuccessCallbackRunnable(mSuccess, mError,
                                                              final.forget()));
    return NS_OK;
  }

private:
  MediaStreamConstraintsInternal mConstraints;
  already_AddRefed<nsIGetUserMediaDevicesSuccessCallback> mSuccess;
  already_AddRefed<nsIDOMGetUserMediaErrorCallback> mError;
  nsRefPtr<MediaManager> mManager;
  uint64_t mWindowId;
};

MediaManager::MediaManager()
  : mMediaThread(nullptr)
  , mMutex("mozilla::MediaManager")
  , mBackend(nullptr) {
  mPrefs.mWidth  = MediaEngine::DEFAULT_VIDEO_WIDTH;
  mPrefs.mHeight = MediaEngine::DEFAULT_VIDEO_HEIGHT;
  mPrefs.mFPS    = MediaEngine::DEFAULT_VIDEO_FPS;
  mPrefs.mMinFPS = MediaEngine::DEFAULT_VIDEO_MIN_FPS;

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
    if (branch) {
      GetPrefs(branch, nullptr);
    }
  }
  LOG(("%s: default prefs: %dx%d @%dfps (min %d)", __FUNCTION__,
       mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS, mPrefs.mMinFPS));
}

NS_IMPL_ISUPPORTS2(MediaManager, nsIMediaManagerService, nsIObserver)

/* static */ StaticRefPtr<MediaManager> MediaManager::sSingleton;

// NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
// thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
// from MediaManager thread.
/* static */  MediaManager*
MediaManager::Get() {
  if (!sSingleton) {
    sSingleton = new MediaManager();

    NS_NewNamedThread("MediaManager", getter_AddRefs(sSingleton->mMediaThread));
    LOG(("New Media thread for gum"));

    NS_ASSERTION(NS_IsMainThread(), "Only create MediaManager on main thread");
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->AddObserver(sSingleton, "xpcom-shutdown", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
      obs->AddObserver(sSingleton, "getUserMedia:revoke", false);
      obs->AddObserver(sSingleton, "phone-state-changed", false);
    }
    // else MediaManager won't work properly and will leak (see bug 837874)
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      prefs->AddObserver("media.navigator.video.default_width", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_height", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_fps", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_minfps", sSingleton, false);
    }
  }
  return sSingleton;
}

/* static */ already_AddRefed<MediaManager>
MediaManager::GetInstance()
{
  // so we can have non-refcounted getters
  nsRefPtr<MediaManager> service = MediaManager::Get();
  return service.forget();
}

/* static */ nsresult
MediaManager::NotifyRecordingStatusChange(nsPIDOMWindow* aWindow,
                                          const nsString& aMsg,
                                          const bool& aIsAudio,
                                          const bool& aIsVideo)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not get the Observer service for GetUserMedia recording notification.");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->SetPropertyAsBool(NS_LITERAL_STRING("isAudio"), aIsAudio);
  props->SetPropertyAsBool(NS_LITERAL_STRING("isVideo"), aIsVideo);

  bool isApp = false;
  nsString requestURL;

  if (nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell()) {
    nsresult rv = docShell->GetIsApp(&isApp);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isApp) {
      rv = docShell->GetAppManifestURL(requestURL);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (!isApp) {
    nsCString pageURL;
    nsCOMPtr<nsIURI> docURI = aWindow->GetDocumentURI();
    NS_ENSURE_TRUE(docURI, NS_ERROR_FAILURE);

    nsresult rv = docURI->GetSpec(pageURL);
    NS_ENSURE_SUCCESS(rv, rv);

    requestURL = NS_ConvertUTF8toUTF16(pageURL);
  }

  props->SetPropertyAsBool(NS_LITERAL_STRING("isApp"), isApp);
  props->SetPropertyAsAString(NS_LITERAL_STRING("requestURL"), requestURL);

  obs->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
		       "recording-device-events",
		       aMsg.get());

  // Forward recording events to parent process.
  // The events are gathered in chrome process and used for recording indicator
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    unused <<
      dom::ContentChild::GetSingleton()->SendRecordingDeviceEvents(aMsg,
                                                                   requestURL,
                                                                   aIsAudio,
                                                                   aIsVideo);
  }

  return NS_OK;
}

/**
 * The entry point for this file. A call from Navigator::mozGetUserMedia
 * will end up here. MediaManager is a singleton that is responsible
 * for handling all incoming getUserMedia calls from every window.
 */
nsresult
MediaManager::GetUserMedia(JSContext* aCx, bool aPrivileged,
  nsPIDOMWindow* aWindow, const MediaStreamConstraints& aRawConstraints,
  nsIDOMGetUserMediaSuccessCallback* aOnSuccess,
  nsIDOMGetUserMediaErrorCallback* aOnError)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  NS_ENSURE_TRUE(aWindow, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnError, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnSuccess, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError(aOnError);

  Maybe<JSAutoCompartment> ac;
  if (aRawConstraints.mAudio.IsObject() || aRawConstraints.mVideo.IsObject()) {
    ac.construct(aCx, (aRawConstraints.mVideo.IsObject()?
                       aRawConstraints.mVideo.GetAsObject() :
                       aRawConstraints.mAudio.GetAsObject()));
  }

  // aRawConstraints has JSObjects in it, so process it by copying it into
  // MediaStreamConstraintsInternal which does not.

  dom::RootedDictionary<MediaStreamConstraintsInternal> c(aCx);

  // TODO: Simplify this part once Bug 767924 is fixed.
  // Since we cannot yet use unions on non-objects, we process the raw object
  // into discrete members for internal use until Bug 767924 is fixed

  nsresult rv;
  nsString unknownConstraintFound;

  if (aRawConstraints.mAudio.IsObject()) {
    JS::Rooted<JS::Value> temp(aCx,
        JS::ObjectValue(*aRawConstraints.mAudio.GetAsObject()));
    bool success = c.mAudiom.Init(aCx, temp);
    NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

    rv = ValidateTrackConstraints(aCx, aRawConstraints.mAudio.GetAsObject(),
                                  c.mAudiom, &unknownConstraintFound);
    NS_ENSURE_SUCCESS(rv, rv);
    c.mAudio = true;
  } else {
    c.mAudio = aRawConstraints.mAudio.GetAsBoolean();
  }
  if (aRawConstraints.mVideo.IsObject()) {
    JS::Rooted<JS::Value> temp(aCx,
        JS::ObjectValue(*aRawConstraints.mVideo.GetAsObject()));
    bool success = c.mVideom.Init(aCx, temp);
    NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

    rv = ValidateTrackConstraints(aCx, aRawConstraints.mVideo.GetAsObject(),
                                  c.mVideom, &unknownConstraintFound);
    NS_ENSURE_SUCCESS(rv, rv);
    c.mVideo = true;
  } else {
    c.mVideo = aRawConstraints.mVideo.GetAsBoolean();
  }
  c.mPicture = aRawConstraints.mPicture;
  c.mFake = aRawConstraints.mFake;

  /**
   * If we were asked to get a picture, before getting a snapshot, we check if
   * the calling page is allowed to open a popup. We do this because
   * {picture:true} will open a new "window" to let the user preview or select
   * an image, on Android. The desktop UI for {picture:true} is TBD, at which
   * may point we can decide whether to extend this test there as well.
   */
#if !defined(MOZ_WEBRTC)
  if (c.mPicture && !aPrivileged) {
    if (aWindow->GetPopupControlState() > openControlled) {
      nsCOMPtr<nsIPopupWindowManager> pm =
        do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);
      if (!pm) {
        return NS_OK;
      }
      uint32_t permission;
      nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
      pm->TestPermission(doc->NodePrincipal(), &permission);
      if (permission == nsIPopupWindowManager::DENY_POPUP) {
        nsGlobalWindow::FirePopupBlockedEvent(
          doc, aWindow, nullptr, EmptyString(), EmptyString()
        );
        return NS_OK;
      }
    }
  }
#endif

  static bool created = false;
  if (!created) {
    // Force MediaManager to startup before we try to access it from other threads
    // Hack: should init singleton earlier unless it's expensive (mem or CPU)
    (void) MediaManager::Get();
#ifdef MOZ_WIDGET_GONK
    // Initialize MediaPermissionManager before send out any permission request.
    (void) MediaPermissionManager::GetInstance();
#endif //MOZ_WIDGET_GONK
  }

  // Store the WindowID in a hash table and mark as active. The entry is removed
  // when this window is closed or navigated away from.
  uint64_t windowID = aWindow->WindowID();
  nsRefPtr<GetUserMediaRunnable> gUMRunnable;
  // This is safe since we're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  StreamListeners* listeners = GetActiveWindows()->Get(windowID);
  if (!listeners) {
    listeners = new StreamListeners;
    GetActiveWindows()->Put(windowID, listeners);
  }

  if (!unknownConstraintFound.IsEmpty()) {
    // An unsupported mandatory constraint was found.
    //
    // We continue to ignore these for now, because we implement just
    // facingMode, which means all existing uses of mandatory width/height would
    // fail on Firefox only otherwise, which is undesirable.
    //
    // There's also basis for always ignoring them in a new proposal.
    // TODO(jib): This is a super-low-risk fix for backport. Clean up later.

    LOG(("Unsupported mandatory constraint: %s\n",
          NS_ConvertUTF16toUTF8(unknownConstraintFound).get()));

    // unknown constraints existed in aRawConstraints only, which is unused
    // from here, so continuing here effectively ignores them, as is desired.
  }

  // Ensure there's a thread for gum to proxy to off main thread
  nsIThread *mediaThread = MediaManager::GetThread();

  // Create a disabled listener to act as a placeholder
  GetUserMediaCallbackMediaStreamListener* listener =
    new GetUserMediaCallbackMediaStreamListener(mediaThread, windowID);

  // No need for locking because we always do this in the main thread.
  listeners->AppendElement(listener);

  // Developer preference for turning off permission check.
  if (Preferences::GetBool("media.navigator.permission.disabled", false)) {
    aPrivileged = true;
  }

  /**
   * Pass runnables along to GetUserMediaRunnable so it can add the
   * MediaStreamListener to the runnable list.
   */
  if (c.mFake) {
    // Fake stream from default backend.
    gUMRunnable = new GetUserMediaRunnable(c, onSuccess.forget(),
      onError.forget(), windowID, listener, mPrefs, new MediaEngineDefault());
  } else {
    // Stream from default device from WebRTC backend.
    gUMRunnable = new GetUserMediaRunnable(c, onSuccess.forget(),
      onError.forget(), windowID, listener, mPrefs);
  }

#ifdef MOZ_B2G_CAMERA
  if (mCameraManager == nullptr) {
    mCameraManager = nsDOMCameraManager::CreateInstance(aWindow);
  }
#endif

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
  if (c.mPicture) {
    // ShowFilePickerForMimeType() must run on the Main Thread! (on Android)
    NS_DispatchToMainThread(gUMRunnable);
    return NS_OK;
  }
#endif
  // XXX No full support for picture in Desktop yet (needs proper UI)
  if (aPrivileged || c.mFake) {
    mMediaThread->Dispatch(gUMRunnable, NS_DISPATCH_NORMAL);
  } else {
    // Ask for user permission, and dispatch runnable (or not) when a response
    // is received via an observer notification. Each call is paired with its
    // runnable by a GUID.
    nsresult rv;
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Generate a call ID.
    nsID id;
    rv = uuidgen->GenerateUUIDInPlace(&id);
    NS_ENSURE_SUCCESS(rv, rv);

    char buffer[NSID_LENGTH];
    id.ToProvidedString(buffer);
    NS_ConvertUTF8toUTF16 callID(buffer);

    // Store the current callback.
    mActiveCallbacks.Put(callID, gUMRunnable);

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    nsRefPtr<GetUserMediaRequest> req = new GetUserMediaRequest(aWindow,
                                                                callID, c);
    obs->NotifyObservers(req, "getUserMedia:request", nullptr);
  }

  return NS_OK;
}

nsresult
MediaManager::GetUserMediaDevices(nsPIDOMWindow* aWindow,
  const MediaStreamConstraintsInternal& aConstraints,
  nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
  nsIDOMGetUserMediaErrorCallback* aOnError)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  NS_ENSURE_TRUE(aOnError, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnSuccess, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError(aOnError);

  nsCOMPtr<nsIRunnable> gUMDRunnable = new GetUserMediaDevicesRunnable(
    aConstraints, onSuccess.forget(), onError.forget(), aWindow->WindowID()
  );

  nsCOMPtr<nsIThread> deviceThread;
  nsresult rv = NS_NewThread(getter_AddRefs(deviceThread));
  NS_ENSURE_SUCCESS(rv, rv);


  deviceThread->Dispatch(gUMDRunnable, NS_DISPATCH_NORMAL);
  return NS_OK;
}

MediaEngine*
MediaManager::GetBackend(uint64_t aWindowId)
{
  // Plugin backends as appropriate. The default engine also currently
  // includes picture support for Android.
  // This IS called off main-thread.
  MutexAutoLock lock(mMutex);
  if (!mBackend) {
#if defined(MOZ_WEBRTC)
  #ifndef MOZ_B2G_CAMERA
    mBackend = new MediaEngineWebRTC();
  #else
    mBackend = new MediaEngineWebRTC(mCameraManager, aWindowId);
  #endif
#else
    mBackend = new MediaEngineDefault();
#endif
  }
  return mBackend;
}

void
MediaManager::OnNavigation(uint64_t aWindowID)
{
  NS_ASSERTION(NS_IsMainThread(), "OnNavigation called off main thread");

  // Invalidate this window. The runnables check this value before making
  // a call to content.

  // This is safe since we're on main-thread, and the windowlist can only
  // be added to from the main-thread
  StreamListeners* listeners = GetWindowListeners(aWindowID);
  if (!listeners) {
    return;
  }

  uint32_t length = listeners->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsRefPtr<GetUserMediaCallbackMediaStreamListener> listener =
      listeners->ElementAt(i);
    if (listener->Stream()) { // aka HasBeenActivate()ed
      listener->Invalidate();
    }
    listener->Remove();
  }
  listeners->Clear();

  RemoveWindowID(aWindowID);
  // listeners has been deleted
}

void
MediaManager::RemoveFromWindowList(uint64_t aWindowID,
  GetUserMediaCallbackMediaStreamListener *aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "RemoveFromWindowList called off main thread");

  // This is defined as safe on an inactive GUMCMSListener
  aListener->Remove(); // really queues the remove

  StreamListeners* listeners = GetWindowListeners(aWindowID);
  if (!listeners) {
    return;
  }
  listeners->RemoveElement(aListener);
  if (listeners->Length() == 0) {
    RemoveWindowID(aWindowID);
    // listeners has been deleted here

    // get outer windowID
    nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(aWindowID));
    if (window) {
      nsPIDOMWindow *outer = window->GetOuterWindow();
      if (outer) {
        uint64_t outerID = outer->WindowID();

        // Notify the UI that this window no longer has gUM active
        char windowBuffer[32];
        PR_snprintf(windowBuffer, sizeof(windowBuffer), "%llu", outerID);
        nsAutoString data;
        data.Append(NS_ConvertUTF8toUTF16(windowBuffer));

        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        obs->NotifyObservers(nullptr, "recording-window-ended", data.get());
        LOG(("Sent recording-window-ended for window %llu (outer %llu)",
             aWindowID, outerID));
      } else {
        LOG(("No outer window for inner %llu", aWindowID));
      }
    } else {
      LOG(("No inner window for %llu", aWindowID));
    }
  }
}

void
MediaManager::GetPref(nsIPrefBranch *aBranch, const char *aPref,
                      const char *aData, int32_t *aVal)
{
  int32_t temp;
  if (aData == nullptr || strcmp(aPref,aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetIntPref(aPref, &temp))) {
      *aVal = temp;
    }
  }
}

void
MediaManager::GetPrefs(nsIPrefBranch *aBranch, const char *aData)
{
  GetPref(aBranch, "media.navigator.video.default_width", aData, &mPrefs.mWidth);
  GetPref(aBranch, "media.navigator.video.default_height", aData, &mPrefs.mHeight);
  GetPref(aBranch, "media.navigator.video.default_fps", aData, &mPrefs.mFPS);
  GetPref(aBranch, "media.navigator.video.default_minfps", aData, &mPrefs.mMinFPS);
}

nsresult
MediaManager::Observe(nsISupports* aSubject, const char* aTopic,
  const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Observer invoked off the main thread");
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch( do_QueryInterface(aSubject) );
    if (branch) {
      GetPrefs(branch,NS_ConvertUTF16toUTF8(aData).get());
      LOG(("%s: %dx%d @%dfps (min %d)", __FUNCTION__,
           mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS, mPrefs.mMinFPS));
    }
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    obs->RemoveObserver(this, "xpcom-shutdown");
    obs->RemoveObserver(this, "getUserMedia:response:allow");
    obs->RemoveObserver(this, "getUserMedia:response:deny");
    obs->RemoveObserver(this, "getUserMedia:revoke");

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      prefs->RemoveObserver("media.navigator.video.default_width", this);
      prefs->RemoveObserver("media.navigator.video.default_height", this);
      prefs->RemoveObserver("media.navigator.video.default_fps", this);
      prefs->RemoveObserver("media.navigator.video.default_minfps", this);
    }

    // Close off any remaining active windows.
    {
      MutexAutoLock lock(mMutex);
      GetActiveWindows()->Clear();
      mActiveCallbacks.Clear();
      LOG(("Releasing MediaManager singleton and thread"));
      sSingleton = nullptr;
    }

    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:allow")) {
    nsString key(aData);
    nsRefPtr<nsRunnable> runnable;
    if (!mActiveCallbacks.Get(key, getter_AddRefs(runnable))) {
      return NS_OK;
    }
    mActiveCallbacks.Remove(key);

    if (aSubject) {
      // A particular device or devices were chosen by the user.
      // NOTE: does not allow setting a device to null; assumes nullptr
      GetUserMediaRunnable* gUMRunnable =
        static_cast<GetUserMediaRunnable*>(runnable.get());

      nsCOMPtr<nsISupportsArray> array(do_QueryInterface(aSubject));
      MOZ_ASSERT(array);
      uint32_t len = 0;
      array->Count(&len);
      MOZ_ASSERT(len);
      if (!len) {
        gUMRunnable->Denied(NS_LITERAL_STRING("PERMISSION_DENIED")); // neither audio nor video were selected
        return NS_OK;
      }
      for (uint32_t i = 0; i < len; i++) {
        nsCOMPtr<nsISupports> supports;
        array->GetElementAt(i,getter_AddRefs(supports));
        nsCOMPtr<nsIMediaDevice> device(do_QueryInterface(supports));
        MOZ_ASSERT(device); // shouldn't be returning anything else...
        if (device) {
          nsString type;
          device->GetType(type);
          if (type.EqualsLiteral("video")) {
            gUMRunnable->SetVideoDevice(static_cast<MediaDevice*>(device.get()));
          } else if (type.EqualsLiteral("audio")) {
            gUMRunnable->SetAudioDevice(static_cast<MediaDevice*>(device.get()));
          } else {
            NS_WARNING("Unknown device type in getUserMedia");
          }
        }
      }
    }

    // Reuse the same thread to save memory.
    mMediaThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:deny")) {
    nsString errorMessage(NS_LITERAL_STRING("PERMISSION_DENIED"));

    if (aSubject) {
      nsCOMPtr<nsISupportsString> msg(do_QueryInterface(aSubject));
      MOZ_ASSERT(msg);
      msg->GetData(errorMessage);
      if (errorMessage.IsEmpty())
        errorMessage.Assign(NS_LITERAL_STRING("UNKNOWN_ERROR"));
    }

    nsString key(aData);
    nsRefPtr<nsRunnable> runnable;
    if (!mActiveCallbacks.Get(key, getter_AddRefs(runnable))) {
      return NS_OK;
    }
    mActiveCallbacks.Remove(key);

    GetUserMediaRunnable* gUMRunnable =
      static_cast<GetUserMediaRunnable*>(runnable.get());
    gUMRunnable->Denied(errorMessage);
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:revoke")) {
    nsresult rv;
    uint64_t windowID = nsString(aData).ToInteger64(&rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv)) {
      LOG(("Revoking MediaCapture access for window %llu",windowID));
      OnNavigation(windowID);
    }

    return NS_OK;
  }
#ifdef MOZ_WIDGET_GONK
  else if (!strcmp(aTopic, "phone-state-changed")) {
    nsString state(aData);
    if (atoi((const char*)state.get()) == nsIAudioManager::PHONE_STATE_IN_CALL) {
      StopMediaStreams();
    }
    return NS_OK;
  }
#endif

  return NS_OK;
}

static PLDHashOperator
WindowsHashToArrayFunc (const uint64_t& aId,
                        StreamListeners* aData,
                        void *userArg)
{
    nsISupportsArray *array =
        static_cast<nsISupportsArray *>(userArg);
    nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(aId));
    (void) aData;

    MOZ_ASSERT(window);
    if (window) {
      array->AppendElement(window);
    }
    return PL_DHASH_NEXT;
}


nsresult
MediaManager::GetActiveMediaCaptureWindows(nsISupportsArray **aArray)
{
  MOZ_ASSERT(aArray);
  nsISupportsArray *array;
  nsresult rv = NS_NewISupportsArray(&array); // AddRefs
  if (NS_FAILED(rv))
    return rv;

  mActiveWindows.EnumerateRead(WindowsHashToArrayFunc, array);

  *aArray = array;
  return NS_OK;
}

NS_IMETHODIMP
MediaManager::MediaCaptureWindowState(nsIDOMWindow* aWindow, bool* aVideo,
                                      bool* aAudio)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  *aVideo = false;
  *aAudio = false;

  nsresult rv = MediaCaptureWindowStateInternal(aWindow, aVideo, aAudio);
#ifdef DEBUG
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
  LOG(("%s: window %lld capturing %s %s", __FUNCTION__, piWin ? piWin->WindowID() : -1,
       *aVideo ? "video" : "", *aAudio ? "audio" : ""));
#endif
  return rv;
}

nsresult
MediaManager::MediaCaptureWindowStateInternal(nsIDOMWindow* aWindow, bool* aVideo,
                                              bool* aAudio)
{
  // We need to return the union of all streams in all innerwindows that
  // correspond to that outerwindow.

  // Iterate the docshell tree to find all the child windows, find
  // all the listeners for each one, get the booleans, and merge the
  // results.
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
  if (piWin) {
    if (piWin->GetCurrentInnerWindow() || piWin->IsInnerWindow()) {
      uint64_t windowID;
      if (piWin->GetCurrentInnerWindow()) {
        windowID = piWin->GetCurrentInnerWindow()->WindowID();
      } else {
        windowID = piWin->WindowID();
      }
      StreamListeners* listeners = GetActiveWindows()->Get(windowID);
      if (listeners) {
        uint32_t length = listeners->Length();
        for (uint32_t i = 0; i < length; ++i) {
          nsRefPtr<GetUserMediaCallbackMediaStreamListener> listener =
            listeners->ElementAt(i);
          if (listener->CapturingVideo()) {
            *aVideo = true;
          }
          if (listener->CapturingAudio()) {
            *aAudio = true;
          }
          if (*aAudio && *aVideo) {
            return NS_OK; // no need to continue iterating
          }
        }
      }
    }

    // iterate any children of *this* window (iframes, etc)
    nsCOMPtr<nsIDocShellTreeNode> node =
      do_QueryInterface(piWin->GetDocShell());
    if (node) {
      int32_t i, count;
      node->GetChildCount(&count);
      for (i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> item;
        node->GetChildAt(i, getter_AddRefs(item));
        nsCOMPtr<nsPIDOMWindow> win = do_GetInterface(item);

        MediaCaptureWindowStateInternal(win, aVideo, aAudio);
        if (*aAudio && *aVideo) {
          return NS_OK; // no need to continue iterating
        }
      }
    }
  }
  return NS_OK;
}

void
MediaManager::StopMediaStreams()
{
  nsCOMPtr<nsISupportsArray> array;
  GetActiveMediaCaptureWindows(getter_AddRefs(array));
  uint32_t len;
  array->Count(&len);
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsISupports> window;
    array->GetElementAt(i, getter_AddRefs(window));
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(window));
    if (win) {
      OnNavigation(win->WindowID());
    }
  }
}

// Can be invoked from EITHER MainThread or MSG thread
void
GetUserMediaCallbackMediaStreamListener::Invalidate()
{

  nsRefPtr<MediaOperationRunnable> runnable;
  // We can't take a chance on blocking here, so proxy this to another
  // thread.
  // Pass a ref to us (which is threadsafe) so it can query us for the
  // source stream info.
  runnable = new MediaOperationRunnable(MEDIA_STOP,
                                        this, nullptr, nullptr,
                                        mAudioSource, mVideoSource,
                                        mFinished, mWindowID);
  mMediaThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

// Called from the MediaStreamGraph thread
void
GetUserMediaCallbackMediaStreamListener::NotifyFinished(MediaStreamGraph* aGraph)
{
  mFinished = true;
  Invalidate(); // we know it's been activated
  NS_DispatchToMainThread(new GetUserMediaListenerRemove(mWindowID, this));
}

// Called from the MediaStreamGraph thread
// this can be in response to our own RemoveListener() (via ::Remove()), or
// because the DOM GC'd the DOMLocalMediaStream/etc we're attached to.
void
GetUserMediaCallbackMediaStreamListener::NotifyRemoved(MediaStreamGraph* aGraph)
{
  {
    MutexAutoLock lock(mLock); // protect access to mRemoved
    MM_LOG(("Listener removed by DOM Destroy(), mFinished = %d", (int) mFinished));
    mRemoved = true;
  }
  if (!mFinished) {
    NotifyFinished(aGraph);
  }
}

NS_IMETHODIMP
GetUserMediaNotificationEvent::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  // Make sure mStream is cleared and our reference to the DOMMediaStream
  // is dropped on the main thread, no matter what happens in this method.
  // Otherwise this object might be destroyed off the main thread,
  // releasing DOMMediaStream off the main thread, which is not allowed.
  nsRefPtr<DOMMediaStream> stream = mStream.forget();

  nsString msg;
  switch (mStatus) {
  case STARTING:
    msg = NS_LITERAL_STRING("starting");
    stream->OnTracksAvailable(mOnTracksAvailableCallback.forget());
    break;
  case STOPPING:
    msg = NS_LITERAL_STRING("shutdown");
    if (mListener) {
      mListener->SetStopped();
    }
    break;
  }

  nsCOMPtr<nsPIDOMWindow> window = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  return MediaManager::NotifyRecordingStatusChange(window, msg, mIsAudio, mIsVideo);
}

} // namespace mozilla
