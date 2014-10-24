/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "MediaStreamGraph.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "GetUserMediaRequest.h"
#include "nsHashPropertyBag.h"
#ifdef MOZ_WIDGET_GONK
#include "nsIAudioManager.h"
#endif
#include "nsIEventTarget.h"
#include "nsIUUIDGenerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPopupWindowManager.h"
#include "nsISupportsArray.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIDNService.h"
#include "nsNetUtil.h"
#include "mozilla/Types.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "mozilla/Preferences.h"
#include "MediaTrackConstraints.h"

#include "Latency.h"

// For PR_snprintf
#include "prprf.h"

#include "nsJSUtils.h"
#include "nsGlobalWindow.h"

/* Using WebRTC backend on Desktops (Mac, Windows, Linux), otherwise default */
#include "MediaEngineDefault.h"
#if defined(MOZ_WEBRTC)
#include "MediaEngineWebRTC.h"
#include "browser_logging/WebRtcLog.h"
#endif

#ifdef MOZ_B2G
#include "MediaPermissionGonk.h"
#endif

#if defined(XP_MACOSX)
#include "nsCocoaFeatures.h"
#endif
#if defined (XP_WIN)
#include "mozilla/WindowsVersion.h"
#endif

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaStream::GetCurrentTime.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// XXX Workaround for bug 986974 to maintain the existing broken semantics
template<>
struct nsIMediaDevice::COMTypeInfo<mozilla::VideoDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::VideoDevice, void>::kIID = NS_IMEDIADEVICE_IID;
template<>
struct nsIMediaDevice::COMTypeInfo<mozilla::AudioDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::AudioDevice, void>::kIID = NS_IMEDIADEVICE_IID;

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
using dom::MediaTrackConstraintSet;        // Mandatory or optional constraints
using dom::MediaTrackConstraints;          // Raw mMandatory (as JSObject)
using dom::GetUserMediaRequest;
using dom::Sequence;
using dom::OwningBooleanOrMediaTrackConstraints;
using dom::SupportedAudioConstraints;
using dom::SupportedVideoConstraints;

static bool
HostInDomain(const nsCString &aHost, const nsCString &aPattern)
{
  PRInt32 patternOffset = 0;
  PRInt32 hostOffset = 0;

  // Act on '*.' wildcard in the left-most position in a domain pattern.
  if (aPattern.Length() > 2 && aPattern[0] == '*' && aPattern[1] == '.') {
    patternOffset = 2;

    // Ignore the lowest level sub-domain for the hostname.
    hostOffset = aHost.FindChar('.') + 1;

    if (hostOffset <= 1) {
      // Reject a match between a wildcard and a TLD or '.foo' form.
      return false;
    }
  }

  nsDependentCString hostRoot(aHost, hostOffset);
  return hostRoot.EqualsIgnoreCase(aPattern.BeginReading() + patternOffset);
}

static bool
HostHasPermission(nsIURI &docURI)
{
  nsresult rv;

  bool isHttps;
  rv = docURI.SchemeIs("https",&isHttps);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  if (!isHttps) {
    return false;
  }

  nsAdoptingCString hostName;
  docURI.GetAsciiHost(hostName); //normalize UTF8 to ASCII equivalent
  nsAdoptingCString domainWhiteList =
    Preferences::GetCString("media.getusermedia.screensharing.allowed_domains");
  domainWhiteList.StripWhitespace();

  if (domainWhiteList.IsEmpty() || hostName.IsEmpty()) {
    return false;
  }

  // Get UTF8 to ASCII domain name normalization service
  nsCOMPtr<nsIIDNService> idnService
    = do_GetService("@mozilla.org/network/idn-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t begin = 0;
  uint32_t end = 0;
  nsCString domainName;
  /*
     Test each domain name in the comma separated list
     after converting from UTF8 to ASCII. Each domain
     must match exactly or have a single leading '*.' wildcard
  */
  do {
    end = domainWhiteList.FindChar(',', begin);
    if (end == (uint32_t)-1) {
      // Last or only domain name in the comma separated list
      end = domainWhiteList.Length();
    }

    rv = idnService->ConvertUTF8toACE(Substring(domainWhiteList, begin, end - begin),
                                      domainName);
    if (NS_SUCCEEDED(rv)) {
      if (HostInDomain(hostName, domainName)) {
        return true;
      }
    } else {
      NS_WARNING("Failed to convert UTF-8 host to ASCII");
    }

    begin = end + 1;
  } while (end < domainWhiteList.Length());

  return false;
}

ErrorCallbackRunnable::ErrorCallbackRunnable(
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback>& aSuccess,
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aError,
  const nsAString& aErrorMsg, uint64_t aWindowID)
  : mErrorMsg(aErrorMsg)
  , mWindowID(aWindowID)
  , mManager(MediaManager::GetInstance())
{
  mSuccess.swap(aSuccess);
  mError.swap(aError);
}

ErrorCallbackRunnable::~ErrorCallbackRunnable()
{
  MOZ_ASSERT(!mSuccess && !mError);
}

NS_IMETHODIMP
ErrorCallbackRunnable::Run()
{
  // Only run if the window is still active.
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success = mSuccess.forget();
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error = mError.forget();

  if (!(mManager->IsWindowStillActive(mWindowID))) {
    return NS_OK;
  }
  // This is safe since we're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  error->OnError(mErrorMsg);
  return NS_OK;
}

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
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback>& aSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aError,
    nsIDOMFile* aFile, uint64_t aWindowID)
    : mFile(aFile)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance())
  {
    mSuccess.swap(aSuccess);
    mError.swap(aError);
  }

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success = mSuccess.forget();
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error = mError.forget();

    if (!(mManager->IsWindowStillActive(mWindowID))) {
      return NS_OK;
    }
    // This is safe since we're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    success->OnSuccess(mFile);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
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
    uint64_t aWindowID,
    nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback>& aSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aError,
    nsTArray<nsRefPtr<MediaDevice>>* aDevices)
    : mDevices(aDevices)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance())
  {
    mSuccess.swap(aSuccess);
    mError.swap(aError);
  }

  NS_IMETHOD
  Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    // Only run if window is still on our active list.
    if (!mManager->IsWindowStillActive(mWindowID)) {
      return NS_OK;
    }

    nsCOMPtr<nsIWritableVariant> devices =
      do_CreateInstance("@mozilla.org/variant;1");

    int32_t len = mDevices->Length();
    if (len == 0) {
      // XXX
      // We should in the future return an empty array, and dynamically add
      // devices to the dropdowns if things are hotplugged while the
      // requester is up.
      mError->OnError(NS_LITERAL_STRING("NO_DEVICES_FOUND"));
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

    mSuccess->OnSuccess(devices);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  nsAutoPtr<nsTArray<nsRefPtr<MediaDevice>>> mDevices;
  uint64_t mWindowID;
  nsRefPtr<MediaManager> mManager;
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
NS_IMPL_ISUPPORTS(MediaDevice, nsIMediaDevice)

MediaDevice::MediaDevice(MediaEngineSource* aSource)
  : mHasFacingMode(false)
  , mSource(aSource) {
  mSource->GetName(mName);
  mSource->GetUUID(mID);
}

VideoDevice::VideoDevice(MediaEngineVideoSource* aSource)
  : MediaDevice(aSource) {
#ifdef MOZ_B2G_CAMERA
  if (mName.EqualsLiteral("back")) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::Environment;
  } else if (mName.EqualsLiteral("front")) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::User;
  }
#endif // MOZ_B2G_CAMERA
#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
  // Names are generated. Example: "Camera 0, Facing back, Orientation 90"
  //
  // See media/webrtc/trunk/webrtc/modules/video_capture/android/java/src/org/
  // webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java

  if (mName.Find(NS_LITERAL_STRING("Facing back")) != kNotFound) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::Environment;
  } else if (mName.Find(NS_LITERAL_STRING("Facing front")) != kNotFound) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::User;
  }
#endif // ANDROID
#ifdef XP_MACOSX
  // Kludge to test user-facing cameras on OSX.
  if (mName.Find(NS_LITERAL_STRING("Face")) != -1) {
    mHasFacingMode = true;
    mFacingMode = dom::VideoFacingModeEnum::User;
  }
#endif
  mMediaSource = aSource->GetMediaSource();
}

/**
 * Helper functions that implement the constraints algorithm from
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#methods-5
 */

// Reminder: add handling for new constraints both here and in GetSources below!

bool
VideoDevice::SatisfiesConstraintSets(
    const nsTArray<const MediaTrackConstraintSet*>& aConstraintSets)
{
  // Interrogate device-inherent properties first.
  for (size_t i = 0; i < aConstraintSets.Length(); i++) {
    auto& c = *aConstraintSets[i];
    if (c.mFacingMode.WasPassed()) {
      nsString s;
      GetFacingMode(s);
      if (!s.EqualsASCII(dom::VideoFacingModeEnumValues::strings[
          static_cast<uint32_t>(c.mFacingMode.Value())].value)) {
        return false;
      }
    }
    nsString s;
    GetMediaSource(s);
    if (!s.EqualsASCII(dom::MediaSourceEnumValues::strings[
        static_cast<uint32_t>(c.mMediaSource)].value)) {
      return false;
    }
  }
  // Forward request to underlying object to interrogate per-mode capabilities.
  return GetSource()->SatisfiesConstraintSets(aConstraintSets);
}

AudioDevice::AudioDevice(MediaEngineAudioSource* aSource)
  : MediaDevice(aSource) {}

bool
AudioDevice::SatisfiesConstraintSets(
    const nsTArray<const MediaTrackConstraintSet*>& aConstraintSets)
{
  // TODO: Add audio-specific constraints
  return true;
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
  return NS_OK;
}

NS_IMETHODIMP
VideoDevice::GetType(nsAString& aType)
{
  aType.AssignLiteral(MOZ_UTF16("video"));
  return NS_OK;
}

NS_IMETHODIMP
AudioDevice::GetType(nsAString& aType)
{
  aType.AssignLiteral(MOZ_UTF16("audio"));
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

NS_IMETHODIMP
MediaDevice::GetMediaSource(nsAString& aMediaSource)
{
  if (mMediaSource == MediaSourceType::Microphone) {
    aMediaSource.Assign(NS_LITERAL_STRING("microphone"));
  } else if (mMediaSource == MediaSourceType::Window) { // this will go away
    aMediaSource.Assign(NS_LITERAL_STRING("window"));
  } else { // all the rest are shared
    aMediaSource.Assign(NS_ConvertUTF8toUTF16(
      dom::MediaSourceEnumValues::strings[uint32_t(mMediaSource)].value));
  }
  return NS_OK;
}

VideoDevice::Source*
VideoDevice::GetSource()
{
  return static_cast<Source*>(&*mSource);
}

AudioDevice::Source*
AudioDevice::GetSource()
{
  return static_cast<Source*>(&*mSource);
}

/**
 * A subclass that we only use to stash internal pointers to MediaStreamGraph objects
 * that need to be cleaned up.
 */
class nsDOMUserMediaStream : public DOMLocalMediaStream
{
public:
  static already_AddRefed<nsDOMUserMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow,
                         GetUserMediaCallbackMediaStreamListener* aListener,
                         MediaEngineSource* aAudioSource,
                         MediaEngineSource* aVideoSource)
  {
    DOMMediaStream::TrackTypeHints hints =
      (aAudioSource ? DOMMediaStream::HINT_CONTENTS_AUDIO : 0) |
      (aVideoSource ? DOMMediaStream::HINT_CONTENTS_VIDEO : 0);

    nsRefPtr<nsDOMUserMediaStream> stream = new nsDOMUserMediaStream(aListener,
                                                                     aAudioSource,
                                                                     aVideoSource);
    stream->InitTrackUnionStream(aWindow, hints);
    return stream.forget();
  }

  nsDOMUserMediaStream(GetUserMediaCallbackMediaStreamListener* aListener,
                       MediaEngineSource *aAudioSource,
                       MediaEngineSource *aVideoSource) :
    mListener(aListener),
    mAudioSource(aAudioSource),
    mVideoSource(aVideoSource),
    mEchoOn(true),
    mAgcOn(false),
    mNoiseOn(true),
#ifdef MOZ_WEBRTC
    mEcho(webrtc::kEcDefault),
    mAgc(webrtc::kAgcDefault),
    mNoise(webrtc::kNsDefault),
#else
    mEcho(0),
    mAgc(0),
    mNoise(0),
#endif
    mPlayoutDelay(20)
  {}

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

  // For gUM streams, we have a trackunion which assigns TrackIDs.  However, for a
  // single-source trackunion like we have here, the TrackUnion will assign trackids
  // that match the source's trackids, so we can avoid needing a mapping function.
  // XXX This will not handle more complex cases well.
  virtual void StopTrack(TrackID aTrackID)
  {
    if (mSourceStream) {
      mSourceStream->EndTrack(aTrackID);
      // We could override NotifyMediaStreamTrackEnded(), and maybe should, but it's
      // risky to do late in a release since that will affect all track ends, and not
      // just StopTrack()s.
      if (GetDOMTrackFor(aTrackID)) {
        mListener->StopTrack(aTrackID, !!GetDOMTrackFor(aTrackID)->AsAudioStreamTrack());
      } else {
        LOG(("StopTrack(%d) on non-existant track", aTrackID));
      }
    }
  }

#if 0
  virtual void NotifyMediaStreamTrackEnded(dom::MediaStreamTrack* aTrack)
  {
    TrackID trackID = aTrack->GetTrackID();
    // We override this so we can also tell the backend to stop capturing if the track ends
    LOG(("track %d ending, type = %s",
         trackID, aTrack->AsAudioStreamTrack() ? "audio" : "video"));
    MOZ_ASSERT(aTrack->AsVideoStreamTrack() || aTrack->AsAudioStreamTrack());
    mListener->StopTrack(trackID, !!aTrack->AsAudioStreamTrack());

    // forward to superclass
    DOMLocalMediaStream::NotifyMediaStreamTrackEnded(aTrack);
  }
#endif

  // Allow getUserMedia to pass input data directly to PeerConnection/MediaPipeline
  virtual bool AddDirectListener(MediaStreamDirectListener *aListener) MOZ_OVERRIDE
  {
    if (mSourceStream) {
      mSourceStream->AddDirectListener(aListener);
      return true; // application should ignore NotifyQueuedTrackData
    }
    return false;
  }

  virtual void
  AudioConfig(bool aEchoOn, uint32_t aEcho,
              bool aAgcOn, uint32_t aAgc,
              bool aNoiseOn, uint32_t aNoise,
              int32_t aPlayoutDelay)
  {
    mEchoOn = aEchoOn;
    mEcho = aEcho;
    mAgcOn = aAgcOn;
    mAgc = aAgc;
    mNoiseOn = aNoiseOn;
    mNoise = aNoise;
    mPlayoutDelay = aPlayoutDelay;
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

  virtual DOMLocalMediaStream* AsDOMLocalMediaStream()
  {
    return this;
  }

  virtual MediaEngineSource* GetMediaEngine(TrackID aTrackID)
  {
    // MediaEngine supports only one video and on video track now and TrackID is
    // fixed in MediaEngine.
    if (aTrackID == kVideoTrack) {
      return mVideoSource;
    }
    else if (aTrackID == kAudioTrack) {
      return mAudioSource;
    }

    return nullptr;
  }

  // The actual MediaStream is a TrackUnionStream. But these resources need to be
  // explicitly destroyed too.
  nsRefPtr<SourceMediaStream> mSourceStream;
  nsRefPtr<MediaInputPort> mPort;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
  nsRefPtr<MediaEngineSource> mAudioSource; // so we can turn on AEC
  nsRefPtr<MediaEngineSource> mVideoSource;
  bool mEchoOn;
  bool mAgcOn;
  bool mNoiseOn;
  uint32_t mEcho;
  uint32_t mAgc;
  uint32_t mNoise;
  uint32_t mPlayoutDelay;
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
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback>& aSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aError,
    uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener* aListener,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource,
    PeerIdentity* aPeerIdentity)
    : mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mWindowID(aWindowID)
    , mListener(aListener)
    , mPeerIdentity(aPeerIdentity)
    , mManager(MediaManager::GetInstance())
  {
    mSuccess.swap(aSuccess);
    mError.swap(aError);
  }

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
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
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
#ifdef MOZ_WEBRTC
    int32_t aec = (int32_t) webrtc::kEcUnchanged;
    int32_t agc = (int32_t) webrtc::kAgcUnchanged;
    int32_t noise = (int32_t) webrtc::kNsUnchanged;
#else
    int32_t aec = 0, agc = 0, noise = 0;
#endif
    bool aec_on = false, agc_on = false, noise_on = false;
    int32_t playout_delay = 0;

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

#ifdef MOZ_WEBRTC
    // Right now these configs are only of use if webrtc is available
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

      if (branch) {
        branch->GetBoolPref("media.getusermedia.aec_enabled", &aec_on);
        branch->GetIntPref("media.getusermedia.aec", &aec);
        branch->GetBoolPref("media.getusermedia.agc_enabled", &agc_on);
        branch->GetIntPref("media.getusermedia.agc", &agc);
        branch->GetBoolPref("media.getusermedia.noise_enabled", &noise_on);
        branch->GetIntPref("media.getusermedia.noise", &noise);
        branch->GetIntPref("media.getusermedia.playout_delay", &playout_delay);
      }
    }
#endif
    // Create a media stream.
    nsRefPtr<nsDOMUserMediaStream> trackunion =
      nsDOMUserMediaStream::CreateTrackUnionStream(window, mListener,
                                                   mAudioSource, mVideoSource);
    if (!trackunion) {
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error = mError.forget();
      LOG(("Returning error for getUserMedia() - no stream"));
      error->OnError(NS_LITERAL_STRING("NO_STREAM"));
      return NS_OK;
    }
    trackunion->AudioConfig(aec_on, (uint32_t) aec,
                            agc_on, (uint32_t) agc,
                            noise_on, (uint32_t) noise,
                            playout_delay);


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

    nsCOMPtr<nsIPrincipal> principal;
    if (mPeerIdentity) {
      principal = do_CreateInstance("@mozilla.org/nullprincipal;1");
      trackunion->SetPeerIdentity(mPeerIdentity.forget());
    } else {
      principal = window->GetExtantDoc()->NodePrincipal();
    }
    trackunion->CombineWithPrincipal(principal);

    // The listener was added at the beginning in an inactive state.
    // Activate our listener. We'll call Start() on the source when get a callback
    // that the MediaStream has started consuming. The listener is freed
    // when the page is invalidated (on navigation or close).
    mListener->Activate(stream.forget(), mAudioSource, mVideoSource);

    // Note: includes JS callbacks; must be released on MainThread
    TracksAvailableCallback* tracksAvailableCallback =
      new TracksAvailableCallback(mManager, mSuccess, mWindowID, trackunion);

    mListener->AudioConfig(aec_on, (uint32_t) aec,
                           agc_on, (uint32_t) agc,
                           noise_on, (uint32_t) noise,
                           playout_delay);

    // Dispatch to the media thread to ask it to start the sources,
    // because that can take a while.
    // Pass ownership of trackunion to the MediaOperationTask
    // to ensure it's kept alive until the MediaOperationTask runs (at least).
    MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
      new MediaOperationTask(MEDIA_START, mListener, trackunion,
                             tracksAvailableCallback,
                             mAudioSource, mVideoSource, false, mWindowID,
                             mError.forget()));

    // We won't need mError now.
    mError = nullptr;
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  nsRefPtr<MediaEngineSource> mAudioSource;
  nsRefPtr<MediaEngineSource> mVideoSource;
  uint64_t mWindowID;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
  nsAutoPtr<PeerIdentity> mPeerIdentity;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

static bool
IsOn(const OwningBooleanOrMediaTrackConstraints &aUnion) {
  return !aUnion.IsBoolean() || aUnion.GetAsBoolean();
}

static const MediaTrackConstraints&
GetInvariant(const OwningBooleanOrMediaTrackConstraints &aUnion) {
  static const MediaTrackConstraints empty;
  return aUnion.IsMediaTrackConstraints() ?
      aUnion.GetAsMediaTrackConstraints() : empty;
}

// Source getter that constrains list returned

template<class DeviceType, class ConstraintsType>
static void
  GetSources(MediaEngine *engine,
             ConstraintsType &aConstraints,
             void (MediaEngine::* aEnumerate)(MediaSourceType,
                 nsTArray<nsRefPtr<typename DeviceType::Source> >*),
             nsTArray<nsRefPtr<DeviceType>>& aResult,
             const char* media_device_name = nullptr)
{
  typedef nsTArray<nsRefPtr<DeviceType>> SourceSet;

  nsString deviceName;
  // First collect sources
  SourceSet candidateSet;
  {
    nsTArray<nsRefPtr<typename DeviceType::Source> > sources;
    // all MediaSourceEnums are contained in MediaSourceType
    (engine->*aEnumerate)((MediaSourceType)((int)aConstraints.mMediaSource), &sources);
    /**
      * We're allowing multiple tabs to access the same camera for parity
      * with Chrome.  See bug 811757 for some of the issues surrounding
      * this decision.  To disallow, we'd filter by IsAvailable() as we used
      * to.
      */
    for (uint32_t len = sources.Length(), i = 0; i < len; i++) {
      sources[i]->GetName(deviceName);
      if (media_device_name && strlen(media_device_name) > 0)  {
        if (deviceName.EqualsASCII(media_device_name)) {
          candidateSet.AppendElement(new DeviceType(sources[i]));
          break;
        }
      } else {
        candidateSet.AppendElement(new DeviceType(sources[i]));
      }
    }
  }

  // Apply constraints to the list of sources.

  auto& c = aConstraints;
  if (c.mUnsupportedRequirement) {
    // Check upfront the names of required constraints that are unsupported for
    // this media-type. The spec requires these to fail, so getting them out of
    // the way early provides a necessary invariant for the remaining algorithm
    // which maximizes code-reuse by ignoring constraints of the other type
    // (specifically, SatisfiesConstraintSets is reused for the advanced algorithm
    // where the spec requires it to ignore constraints of the other type)
    return;
  }

  // Now on to the actual algorithm: First apply required constraints.

  // Stack constraintSets that pass, starting with the required one, because the
  // whole stack must be re-satisfied each time a capability-set is ruled out
  // (this avoids storing state and pushing algorithm into the lower-level code).
  nsTArray<const MediaTrackConstraintSet*> aggregateConstraints;
  aggregateConstraints.AppendElement(&c.mRequired);

  for (uint32_t i = 0; i < candidateSet.Length();) {
    // Overloading instead of template specialization keeps things local
    if (!candidateSet[i]->SatisfiesConstraintSets(aggregateConstraints)) {
      candidateSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }

  // TODO(jib): Proper non-ordered handling of nonrequired constraints (907352)
  //
  // For now, put nonrequired constraints at tail of Advanced list.
  // This isn't entirely accurate, as order will matter, but few will notice
  // the difference until we get camera selection and a few more constraints.
  if (c.mNonrequired.Length()) {
    if (!c.mAdvanced.WasPassed()) {
      c.mAdvanced.Construct();
    }
    c.mAdvanced.Value().MoveElementsFrom(c.mNonrequired);
  }

  // Then apply advanced (formerly known as optional) constraints.
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

  if (c.mAdvanced.WasPassed()) {
    auto &array = c.mAdvanced.Value();

    for (int i = 0; i < int(array.Length()); i++) {
      aggregateConstraints.AppendElement(&array[i]);
      SourceSet rejects;
      for (uint32_t j = 0; j < candidateSet.Length();) {
        if (!candidateSet[j]->SatisfiesConstraintSets(aggregateConstraints)) {
          rejects.AppendElement(candidateSet[j]);
          candidateSet.RemoveElementAt(j);
        } else {
          ++j;
        }
      }
      (candidateSet.Length()? tailSet : candidateSet).MoveElementsFrom(rejects);
      if (!candidateSet.Length()) {
        aggregateConstraints.RemoveElementAt(aggregateConstraints.Length() - 1);
      }
    }
  }

  // TODO: Proper non-ordered handling of nonrequired constraints (Bug 907352)

  aResult.MoveElementsFrom(candidateSet);
  aResult.MoveElementsFrom(tailSet);
}

/**
 * Runs on a seperate thread and is responsible for enumerating devices.
 * Depending on whether a picture or stream was asked for, either
 * ProcessGetUserMedia is called, and the results are sent back to the DOM.
 *
 * Do not run this on the main thread. The success and error callbacks *MUST*
 * be dispatched on the main thread!
 */
class GetUserMediaTask : public Task
{
public:
  GetUserMediaTask(
    const MediaStreamConstraints& aConstraints,
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
    , mBackend(nullptr)
    , mManager(MediaManager::GetInstance())
  {}

  /**
   * The caller can also choose to provide their own backend instead of
   * using the one provided by MediaManager::GetBackend.
   */
  GetUserMediaTask(
    const MediaStreamConstraints& aConstraints,
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
    , mBackend(aBackend)
    , mManager(MediaManager::GetInstance())
  {}

  ~GetUserMediaTask() {
  }

  void
  Fail(const nsAString& aMessage) {
    nsRefPtr<ErrorCallbackRunnable> runnable =
      new ErrorCallbackRunnable(mSuccess, mError, aMessage, mWindowID);
    // These should be empty now
    MOZ_ASSERT(!mSuccess);
    MOZ_ASSERT(!mError);

    NS_DispatchToMainThread(runnable);
  }

  void
  Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");
    MOZ_ASSERT(mSuccess);
    MOZ_ASSERT(mError);

    MediaEngine* backend = mBackend;
    // Was a backend provided?
    if (!backend) {
      backend = mManager->GetBackend(mWindowID);
    }

    // Was a device provided?
    if (!mDeviceChosen) {
      nsresult rv = SelectDevice(backend);
      if (rv != NS_OK) {
        return;
      }
    }

    // There's a bug in the permission code that can leave us with mAudio but no audio device
    ProcessGetUserMedia(((IsOn(mConstraints.mAudio) && mAudioDevice) ?
                         mAudioDevice->GetSource() : nullptr),
                        ((IsOn(mConstraints.mVideo) && mVideoDevice) ?
                         mVideoDevice->GetSource() : nullptr));
  }

  nsresult
  Denied(const nsAString& aErrorMsg)
  {
    MOZ_ASSERT(mSuccess);
    MOZ_ASSERT(mError);

    // We add a disabled listener to the StreamListeners array until accepted
    // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      // This is safe since we're on main-thread, and the window can only
      // be invalidated from the main-thread (see OnNavigation)
      nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success = mSuccess.forget();
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> error = mError.forget();
      error->OnError(aErrorMsg);

      // Should happen *after* error runs for consistency, but may not matter
      nsRefPtr<MediaManager> manager(MediaManager::GetInstance());
      manager->RemoveFromWindowList(mWindowID, mListener);
    } else {
      // This will re-check the window being alive on main-thread
      // Note: we must remove the listener on MainThread as well
      Fail(aErrorMsg);

      // MUST happen after ErrorCallbackRunnable Run()s, as it checks the active window list
      NS_DispatchToMainThread(new GetUserMediaListenerRemove(mWindowID, mListener));
    }

    MOZ_ASSERT(!mSuccess);
    MOZ_ASSERT(!mError);

    return NS_OK;
  }

  nsresult
  SetContraints(const MediaStreamConstraints& aConstraints)
  {
    mConstraints = aConstraints;
    return NS_OK;
  }

  nsresult
  SetAudioDevice(AudioDevice* aAudioDevice)
  {
    mAudioDevice = aAudioDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SetVideoDevice(VideoDevice* aVideoDevice)
  {
    mVideoDevice = aVideoDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SelectDevice(MediaEngine* backend)
  {
    MOZ_ASSERT(mSuccess);
    MOZ_ASSERT(mError);
    if (IsOn(mConstraints.mVideo)) {
      VideoTrackConstraintsN constraints(GetInvariant(mConstraints.mVideo));
      nsTArray<nsRefPtr<VideoDevice>> sources;
      GetSources(backend, constraints, &MediaEngine::EnumerateVideoDevices, sources);

      if (!sources.Length()) {
        Fail(NS_LITERAL_STRING("NO_DEVICES_FOUND"));
        return NS_ERROR_FAILURE;
      }
      // Pick the first available device.
      mVideoDevice = sources[0];
      LOG(("Selected video device"));
    }
    if (IsOn(mConstraints.mAudio)) {
      AudioTrackConstraintsN constraints(GetInvariant(mConstraints.mAudio));
      nsTArray<nsRefPtr<AudioDevice>> sources;
      GetSources(backend, constraints, &MediaEngine::EnumerateAudioDevices, sources);

      if (!sources.Length()) {
        Fail(NS_LITERAL_STRING("NO_DEVICES_FOUND"));
        return NS_ERROR_FAILURE;
      }
      // Pick the first available device.
      mAudioDevice = sources[0];
      LOG(("Selected audio device"));
    }

    return NS_OK;
  }

  /**
   * Allocates a video or audio device and returns a MediaStream via
   * a GetUserMediaStreamRunnable. Runs off the main thread.
   */
  void
  ProcessGetUserMedia(MediaEngineAudioSource* aAudioSource,
                      MediaEngineVideoSource* aVideoSource)
  {
    MOZ_ASSERT(mSuccess);
    MOZ_ASSERT(mError);
    nsresult rv;
    if (aAudioSource) {
      rv = aAudioSource->Allocate(GetInvariant(mConstraints.mAudio), mPrefs);
      if (NS_FAILED(rv)) {
        LOG(("Failed to allocate audiosource %d",rv));
        Fail(NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"));
        return;
      }
    }
    if (aVideoSource) {
      rv = aVideoSource->Allocate(GetInvariant(mConstraints.mVideo), mPrefs);
      if (NS_FAILED(rv)) {
        LOG(("Failed to allocate videosource %d\n",rv));
        if (aAudioSource) {
          aAudioSource->Deallocate();
        }
        Fail(NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"));
        return;
      }
    }
    PeerIdentity* peerIdentity = nullptr;
    if (!mConstraints.mPeerIdentity.IsEmpty()) {
      peerIdentity = new PeerIdentity(mConstraints.mPeerIdentity);
    }

    NS_DispatchToMainThread(new GetUserMediaStreamRunnable(
      mSuccess, mError, mWindowID, mListener, aAudioSource, aVideoSource,
      peerIdentity
    ));

    MOZ_ASSERT(!mSuccess);
    MOZ_ASSERT(!mError);

    return;
  }

private:
  MediaStreamConstraints mConstraints;

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  uint64_t mWindowID;
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener;
  nsRefPtr<AudioDevice> mAudioDevice;
  nsRefPtr<VideoDevice> mVideoDevice;
  MediaEnginePrefs mPrefs;

  bool mDeviceChosen;

  RefPtr<MediaEngine> mBackend;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
class GetUserMediaRunnableWrapper : public nsRunnable
{
public:
  // This object must take ownership of task
  GetUserMediaRunnableWrapper(GetUserMediaTask* task) :
    mTask(task) {
  }

  ~GetUserMediaRunnableWrapper() {
  }

  NS_IMETHOD Run() {
    mTask->Run();
    return NS_OK;
  }

private:
  nsAutoPtr<GetUserMediaTask> mTask;
};
#endif

/**
 * Similar to GetUserMediaTask, but used for the chrome-only
 * GetUserMediaDevices function. Enumerates a list of audio & video devices,
 * wraps them up in nsIMediaDevice objects and returns it to the success
 * callback.
 */
class GetUserMediaDevicesTask : public Task
{
public:
  GetUserMediaDevicesTask(
    const MediaStreamConstraints& aConstraints,
    already_AddRefed<nsIGetUserMediaDevicesSuccessCallback> aSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError,
    uint64_t aWindowId, nsACString& aAudioLoopbackDev,
    nsACString& aVideoLoopbackDev)
    : mConstraints(aConstraints)
    , mSuccess(aSuccess)
    , mError(aError)
    , mManager(MediaManager::GetInstance())
    , mWindowId(aWindowId)
    , mLoopbackAudioDevice(aAudioLoopbackDev)
    , mLoopbackVideoDevice(aVideoLoopbackDev) {}

  void // NS_IMETHOD
  Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

    nsRefPtr<MediaEngine> backend;
    if (mConstraints.mFake)
      backend = new MediaEngineDefault();
    else
      backend = mManager->GetBackend(mWindowId);

    typedef nsTArray<nsRefPtr<MediaDevice>> SourceSet;

    ScopedDeletePtr<SourceSet> final(new SourceSet);
    if (IsOn(mConstraints.mVideo)) {
      VideoTrackConstraintsN constraints(GetInvariant(mConstraints.mVideo));
      nsTArray<nsRefPtr<VideoDevice>> s;
      GetSources(backend, constraints, &MediaEngine::EnumerateVideoDevices, s,
                 mLoopbackVideoDevice.get());
      for (uint32_t i = 0; i < s.Length(); i++) {
        final->AppendElement(s[i]);
      }
    }
    if (IsOn(mConstraints.mAudio)) {
      AudioTrackConstraintsN constraints(GetInvariant(mConstraints.mAudio));
      nsTArray<nsRefPtr<AudioDevice>> s;
      GetSources(backend, constraints, &MediaEngine::EnumerateAudioDevices, s,
                 mLoopbackAudioDevice.get());
      for (uint32_t i = 0; i < s.Length(); i++) {
        final->AppendElement(s[i]);
      }
    }

    NS_DispatchToMainThread(new DeviceSuccessCallbackRunnable(mWindowId,
                                                              mSuccess, mError,
                                                              final.forget()));
    // DeviceSuccessCallbackRunnable should have taken these.
    MOZ_ASSERT(!mSuccess && !mError);
  }

private:
  MediaStreamConstraints mConstraints;
  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  nsRefPtr<MediaManager> mManager;
  uint64_t mWindowId;
  const nsString mCallId;
  // Audio & Video loopback devices to be used based on
  // the preference settings. This is currently used for
  // automated media tests only.
  nsCString mLoopbackAudioDevice;
  nsCString mLoopbackVideoDevice;
};

MediaManager::MediaManager()
  : mMediaThread(nullptr)
  , mMutex("mozilla::MediaManager")
  , mBackend(nullptr) {
  mPrefs.mWidth  = 0; // adaptive default
  mPrefs.mHeight = 0; // adaptive default
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

NS_IMPL_ISUPPORTS(MediaManager, nsIMediaManagerService, nsIObserver)

/* static */ StaticRefPtr<MediaManager> MediaManager::sSingleton;

// NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
// thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
// from MediaManager thread.
/* static */  MediaManager*
MediaManager::Get() {
  if (!sSingleton) {
    NS_ASSERTION(NS_IsMainThread(), "Only create MediaManager on main thread");

    sSingleton = new MediaManager();

    sSingleton->mMediaThread = new base::Thread("MediaManager");
    base::Thread::Options options;
#if defined(_WIN32)
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINUITHREAD;
#else
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINTHREAD;
#endif
    if (!sSingleton->mMediaThread->StartWithOptions(options)) {
      MOZ_CRASH();
    }

    LOG(("New Media thread for gum"));

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

/* static */
MessageLoop*
MediaManager::GetMessageLoop()
{
  NS_ASSERTION(Get(), "MediaManager singleton?");
  NS_ASSERTION(Get()->mMediaThread, "No thread yet");
  return Get()->mMediaThread->message_loop();
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
MediaManager::GetUserMedia(
  nsPIDOMWindow* aWindow, const MediaStreamConstraints& aConstraints,
  nsIDOMGetUserMediaSuccessCallback* aOnSuccess,
  nsIDOMGetUserMediaErrorCallback* aOnError)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  NS_ENSURE_TRUE(aWindow, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnError, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnSuccess, NS_ERROR_NULL_POINTER);

  bool privileged = nsContentUtils::IsChromeDoc(aWindow->GetExtantDoc());

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError(aOnError);

  MediaStreamConstraints c(aConstraints); // copy

  static bool created = false;
  if (!created) {
    // Force MediaManager to startup before we try to access it from other threads
    // Hack: should init singleton earlier unless it's expensive (mem or CPU)
    (void) MediaManager::Get();
#ifdef MOZ_B2G
    // Initialize MediaPermissionManager before send out any permission request.
    (void) MediaPermissionManager::GetInstance();
#endif //MOZ_B2G
  }

  // Store the WindowID in a hash table and mark as active. The entry is removed
  // when this window is closed or navigated away from.
  uint64_t windowID = aWindow->WindowID();
  // This is safe since we're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  StreamListeners* listeners = GetActiveWindows()->Get(windowID);
  if (!listeners) {
    listeners = new StreamListeners;
    GetActiveWindows()->Put(windowID, listeners);
  }

  // Create a disabled listener to act as a placeholder
  GetUserMediaCallbackMediaStreamListener* listener =
    new GetUserMediaCallbackMediaStreamListener(mMediaThread, windowID);

  // No need for locking because we always do this in the main thread.
  listeners->AppendElement(listener);

  // Developer preference for turning off permission check.
  if (Preferences::GetBool("media.navigator.permission.disabled", false)) {
    privileged = true;
  }
  if (!Preferences::GetBool("media.navigator.video.enabled", true)) {
    c.mVideo.SetAsBoolean() = false;
  }

#if defined(ANDROID) || defined(MOZ_WIDGET_GONK)
  // Be backwards compatible only on mobile and only for facingMode.
  if (c.mVideo.IsMediaTrackConstraints()) {
    auto& tc = c.mVideo.GetAsMediaTrackConstraints();
    if (!tc.mRequire.WasPassed() &&
        tc.mMandatory.mFacingMode.WasPassed() && !tc.mFacingMode.WasPassed()) {
      tc.mFacingMode.Construct(tc.mMandatory.mFacingMode.Value());
      tc.mRequire.Construct().AppendElement(NS_LITERAL_STRING("facingMode"));
    }
    if (tc.mOptional.WasPassed() && !tc.mAdvanced.WasPassed()) {
      tc.mAdvanced.Construct();
      for (uint32_t i = 0; i < tc.mOptional.Value().Length(); i++) {
        if (tc.mOptional.Value()[i].mFacingMode.WasPassed()) {
          MediaTrackConstraintSet n;
          n.mFacingMode.Construct(tc.mOptional.Value()[i].mFacingMode.Value());
          tc.mAdvanced.Value().AppendElement(n);
        }
      }
    }
  }
#endif

  if (c.mVideo.IsMediaTrackConstraints() && !privileged) {
    auto& tc = c.mVideo.GetAsMediaTrackConstraints();
    // only allow privileged content to set the window id
    if (tc.mBrowserWindow.WasPassed()) {
      tc.mBrowserWindow.Construct(-1);
    }

    if (tc.mAdvanced.WasPassed()) {
      uint32_t length = tc.mAdvanced.Value().Length();
      for (uint32_t i = 0; i < length; i++) {
        if (tc.mAdvanced.Value()[i].mBrowserWindow.WasPassed()) {
          tc.mAdvanced.Value()[i].mBrowserWindow.Construct(-1);
        }
      }
    }
  }

  // Pass callbacks and MediaStreamListener along to GetUserMediaTask.
  nsAutoPtr<GetUserMediaTask> task;
  if (c.mFake) {
    // Fake stream from default backend.
    task = new GetUserMediaTask(c, onSuccess.forget(),
      onError.forget(), windowID, listener, mPrefs, new MediaEngineDefault());
  } else {
    // Stream from default device from WebRTC backend.
    task = new GetUserMediaTask(c, onSuccess.forget(),
      onError.forget(), windowID, listener, mPrefs);
  }

  nsIURI* docURI = aWindow->GetDocumentURI();

  if (c.mVideo.IsMediaTrackConstraints()) {
    auto& tc = c.mVideo.GetAsMediaTrackConstraints();
    // deny screensharing request if support is disabled
    if (tc.mMediaSource != dom::MediaSourceEnum::Camera) {
      if (tc.mMediaSource == dom::MediaSourceEnum::Browser) {
        if (!Preferences::GetBool("media.getusermedia.browser.enabled", false)) {
          return task->Denied(NS_LITERAL_STRING("PERMISSION_DENIED"));
        }
      } else if (!Preferences::GetBool("media.getusermedia.screensharing.enabled", false)) {
        return task->Denied(NS_LITERAL_STRING("PERMISSION_DENIED"));
      }
      /* Deny screensharing if the requesting document is not from a host
       on the whitelist. */
      // Block screen/window sharing on Mac OSX 10.6 and WinXP until proved that they work
      if (
#if defined(XP_MACOSX) || defined(XP_WIN)
          (
            !Preferences::GetBool("media.getusermedia.screensharing.allow_on_old_platforms", false) &&
#if defined(XP_MACOSX)
            !nsCocoaFeatures::OnLionOrLater()
#endif
#if defined (XP_WIN)
            !IsVistaOrLater()
#endif
           ) ||
#endif
          (!privileged && !HostHasPermission(*docURI))) {
        return task->Denied(NS_LITERAL_STRING("PERMISSION_DENIED"));
      }
    }
  }

#ifdef MOZ_B2G_CAMERA
  if (mCameraManager == nullptr) {
    mCameraManager = nsDOMCameraManager::CreateInstance(aWindow);
  }
#endif

  bool isLoop = false;
  nsCOMPtr<nsIURI> loopURI;
  nsresult rv = NS_NewURI(getter_AddRefs(loopURI), "about:loopconversation");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = docURI->EqualsExceptRef(loopURI, &isLoop);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isLoop) {
    privileged = true;
  }

  // XXX No full support for picture in Desktop yet (needs proper UI)
  if (privileged ||
      (c.mFake && !Preferences::GetBool("media.navigator.permission.fake"))) {
    MediaManager::GetMessageLoop()->PostTask(FROM_HERE, task.forget());
  } else {
    bool isHTTPS = false;
    if (docURI) {
      docURI->SchemeIs("https", &isHTTPS);
    }

    // Check if this site has persistent permissions.
    nsresult rv;
    nsCOMPtr<nsIPermissionManager> permManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t audioPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mAudio)) {
      rv = permManager->TestExactPermissionFromPrincipal(
        aWindow->GetExtantDoc()->NodePrincipal(), "microphone", &audioPerm);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    uint32_t videoPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mVideo)) {
      rv = permManager->TestExactPermissionFromPrincipal(
        aWindow->GetExtantDoc()->NodePrincipal(), "camera", &videoPerm);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if ((!IsOn(c.mAudio) || audioPerm == nsIPermissionManager::DENY_ACTION) &&
        (!IsOn(c.mVideo) || videoPerm == nsIPermissionManager::DENY_ACTION)) {
      return task->Denied(NS_LITERAL_STRING("PERMISSION_DENIED"));
    }

    // Ask for user permission, and dispatch task (or not) when a response
    // is received via an observer notification. Each call is paired with its
    // task by a GUID.
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

    // Store the current unarmed task w/callbacks.
    mActiveCallbacks.Put(callID, task.forget());

    // Add a WindowID cross-reference so OnNavigation can tear things down
    nsTArray<nsString>* array;
    if (!mCallIds.Get(windowID, &array)) {
      array = new nsTArray<nsString>();
      array->AppendElement(callID);
      mCallIds.Put(windowID, array);
    } else {
      array->AppendElement(callID);
    }
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    nsRefPtr<GetUserMediaRequest> req = new GetUserMediaRequest(aWindow,
                                                                callID, c, isHTTPS);
    obs->NotifyObservers(req, "getUserMedia:request", nullptr);
  }

#ifdef MOZ_WEBRTC
  EnableWebRtcLog();
#endif

  return NS_OK;
}

nsresult
MediaManager::GetUserMediaDevices(nsPIDOMWindow* aWindow,
  const MediaStreamConstraints& aConstraints,
  nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
  nsIDOMGetUserMediaErrorCallback* aOnError,
  uint64_t aInnerWindowID)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  NS_ENSURE_TRUE(aOnError, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aOnSuccess, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onError(aOnError);

  // Check if the preference for using loopback devices is enabled.
  nsAdoptingCString loopbackAudioDevice =
    Preferences::GetCString("media.audio_loopback_dev");
  nsAdoptingCString loopbackVideoDevice =
    Preferences::GetCString("media.video_loopback_dev");

  MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
    new GetUserMediaDevicesTask(
      aConstraints, onSuccess.forget(), onError.forget(),
      (aInnerWindowID ? aInnerWindowID : aWindow->WindowID()),
      loopbackAudioDevice, loopbackVideoDevice));

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
    mBackend = new MediaEngineWebRTC(mPrefs);
#else
    mBackend = new MediaEngineDefault();
#endif
  }
  return mBackend;
}

static void
StopSharingCallback(MediaManager *aThis,
                    uint64_t aWindowID,
                    StreamListeners *aListeners,
                    void *aData)
{
  if (aListeners) {
    auto length = aListeners->Length();
    for (size_t i = 0; i < length; ++i) {
      GetUserMediaCallbackMediaStreamListener *listener = aListeners->ElementAt(i);

      if (listener->Stream()) { // aka HasBeenActivate()ed
        listener->Invalidate();
      }
      listener->Remove();
      listener->StopScreenWindowSharing();
    }
    aListeners->Clear();
    aThis->RemoveWindowID(aWindowID);
  }
}


void
MediaManager::OnNavigation(uint64_t aWindowID)
{
  NS_ASSERTION(NS_IsMainThread(), "OnNavigation called off main thread");
  LOG(("OnNavigation for %llu", aWindowID));

  // Invalidate this window. The runnables check this value before making
  // a call to content.

  nsTArray<nsString>* callIds;
  if (mCallIds.Get(aWindowID, &callIds)) {
    for (int i = 0, len = callIds->Length(); i < len; ++i) {
      mActiveCallbacks.Remove((*callIds)[i]);
    }
    mCallIds.Remove(aWindowID);
  }

  // This is safe since we're on main-thread, and the windowlist can only
  // be added to from the main-thread
  nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(aWindowID));
  if (window) {
    IterateWindowListeners(window, StopSharingCallback, nullptr);
  } else {
    RemoveWindowID(aWindowID);
  }
}

void
MediaManager::RemoveWindowID(uint64_t aWindowId)
{
  mActiveWindows.Remove(aWindowId);

  // get outer windowID
  nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
    (nsGlobalWindow::GetInnerWindowWithId(aWindowId));
  if (!window) {
    LOG(("No inner window for %llu", aWindowId));
    return;
  }

  nsPIDOMWindow *outer = window->GetOuterWindow();
  if (!outer) {
    LOG(("No outer window for inner %llu", aWindowId));
    return;
  }

  uint64_t outerID = outer->WindowID();

  // Notify the UI that this window no longer has gUM active
  char windowBuffer[32];
  PR_snprintf(windowBuffer, sizeof(windowBuffer), "%llu", outerID);
  nsString data = NS_ConvertUTF8toUTF16(windowBuffer);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->NotifyObservers(nullptr, "recording-window-ended", data.get());
  LOG(("Sent recording-window-ended for window %llu (outer %llu)",
       aWindowId, outerID));
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
MediaManager::GetPrefBool(nsIPrefBranch *aBranch, const char *aPref,
                          const char *aData, bool *aVal)
{
  bool temp;
  if (aData == nullptr || strcmp(aPref,aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetBoolPref(aPref, &temp))) {
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
  const char16_t* aData)
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
      mCallIds.Clear();
      LOG(("Releasing MediaManager singleton and thread"));
      // Note: won't be released immediately as the Observer has a ref to us
      sSingleton = nullptr;
      if (mMediaThread) {
        mMediaThread->Stop();
      }
      mBackend = nullptr;
    }

    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:allow")) {
    nsString key(aData);
    nsAutoPtr<GetUserMediaTask> task;
    mActiveCallbacks.RemoveAndForget(key, task);
    if (!task) {
      return NS_OK;
    }

    if (aSubject) {
      // A particular device or devices were chosen by the user.
      // NOTE: does not allow setting a device to null; assumes nullptr
      nsCOMPtr<nsISupportsArray> array(do_QueryInterface(aSubject));
      MOZ_ASSERT(array);
      uint32_t len = 0;
      array->Count(&len);
      MOZ_ASSERT(len);
      if (!len) {
        // neither audio nor video were selected
        task->Denied(NS_LITERAL_STRING("PERMISSION_DENIED"));
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
            task->SetVideoDevice(static_cast<VideoDevice*>(device.get()));
          } else if (type.EqualsLiteral("audio")) {
            task->SetAudioDevice(static_cast<AudioDevice*>(device.get()));
          } else {
            NS_WARNING("Unknown device type in getUserMedia");
          }
        }
      }
    }

    // Reuse the same thread to save memory.
    MediaManager::GetMessageLoop()->PostTask(FROM_HERE, task.forget());
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:deny")) {
    nsString errorMessage(NS_LITERAL_STRING("PERMISSION_DENIED"));

    if (aSubject) {
      nsCOMPtr<nsISupportsString> msg(do_QueryInterface(aSubject));
      MOZ_ASSERT(msg);
      msg->GetData(errorMessage);
      if (errorMessage.IsEmpty())
        errorMessage.AssignLiteral(MOZ_UTF16("UNKNOWN_ERROR"));
    }

    nsString key(aData);
    nsAutoPtr<GetUserMediaTask> task;
    mActiveCallbacks.RemoveAndForget(key, task);
    if (task) {
      task->Denied(errorMessage);
    }
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:revoke")) {
    nsresult rv;
    // may be windowid or screen:windowid
    nsDependentString data(aData);
    if (Substring(data, 0, strlen("screen:")).EqualsLiteral("screen:")) {
      uint64_t windowID = PromiseFlatString(Substring(data, strlen("screen:"))).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG(("Revoking Screeen/windowCapture access for window %llu", windowID));
        StopScreensharing(windowID);
      }
    } else {
      uint64_t windowID = nsString(aData).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG(("Revoking MediaCapture access for window %llu", windowID));
        OnNavigation(windowID);
      }
    }
    return NS_OK;
  }
#ifdef MOZ_WIDGET_GONK
  else if (!strcmp(aTopic, "phone-state-changed")) {
    nsString state(aData);
    nsresult rv;
    uint32_t phoneState = state.ToInteger(&rv);

    if (NS_SUCCEEDED(rv) && phoneState == nsIAudioManager::PHONE_STATE_IN_CALL) {
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

  MOZ_ASSERT(window);
  if (window) {
    // mActiveWindows contains both windows that have requested device
    // access and windows that are currently capturing media. We want
    // to return only the latter. See bug 975177.
    bool capturing = false;
    if (aData) {
      uint32_t length = aData->Length();
      for (uint32_t i = 0; i < length; ++i) {
        nsRefPtr<GetUserMediaCallbackMediaStreamListener> listener =
          aData->ElementAt(i);
        if (listener->CapturingVideo() || listener->CapturingAudio() ||
            listener->CapturingScreen() || listener->CapturingWindow() ||
            listener->CapturingApplication()) {
          capturing = true;
          break;
        }
      }
    }

    if (capturing)
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

// XXX flags might be better...
struct CaptureWindowStateData {
  bool *mVideo;
  bool *mAudio;
  bool *mScreenShare;
  bool *mWindowShare;
  bool *mAppShare;
};

static void
CaptureWindowStateCallback(MediaManager *aThis,
                           uint64_t aWindowID,
                           StreamListeners *aListeners,
                           void *aData)
{
  struct CaptureWindowStateData *data = (struct CaptureWindowStateData *) aData;

  if (aListeners) {
    auto length = aListeners->Length();
    for (size_t i = 0; i < length; ++i) {
      GetUserMediaCallbackMediaStreamListener *listener = aListeners->ElementAt(i);

      if (listener->CapturingVideo()) {
        *data->mVideo = true;
      }
      if (listener->CapturingAudio()) {
        *data->mAudio = true;
      }
      if (listener->CapturingScreen()) {
        *data->mScreenShare = true;
      }
      if (listener->CapturingWindow()) {
        *data->mWindowShare = true;
      }
      if (listener->CapturingApplication()) {
        *data->mAppShare = true;
      }
    }
  }
}


NS_IMETHODIMP
MediaManager::MediaCaptureWindowState(nsIDOMWindow* aWindow, bool* aVideo,
                                      bool* aAudio, bool *aScreenShare,
                                      bool* aWindowShare, bool *aAppShare)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  struct CaptureWindowStateData data;
  data.mVideo = aVideo;
  data.mAudio = aAudio;
  data.mScreenShare = aScreenShare;
  data.mWindowShare = aWindowShare;
  data.mAppShare = aAppShare;

  *aVideo = false;
  *aAudio = false;
  *aScreenShare = false;
  *aWindowShare = false;
  *aAppShare = false;

  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
  if (piWin) {
    IterateWindowListeners(piWin, CaptureWindowStateCallback, &data);
  }
#ifdef DEBUG
  LOG(("%s: window %lld capturing %s %s %s %s %s", __FUNCTION__, piWin ? piWin->WindowID() : -1,
       *aVideo ? "video" : "", *aAudio ? "audio" : "",
       *aScreenShare ? "screenshare" : "",  *aWindowShare ? "windowshare" : "",
       *aAppShare ? "appshare" : ""));
#endif
  return NS_OK;
}

static void
StopScreensharingCallback(MediaManager *aThis,
                          uint64_t aWindowID,
                          StreamListeners *aListeners,
                          void *aData)
{
  if (aListeners) {
    auto length = aListeners->Length();
    for (size_t i = 0; i < length; ++i) {
      aListeners->ElementAt(i)->StopScreenWindowSharing();
    }
  }
}

void
MediaManager::StopScreensharing(uint64_t aWindowID)
{
  // We need to stop window/screensharing for all streams in all innerwindows that
  // correspond to that outerwindow.

  nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(aWindowID));
  if (!window) {
    return;
  }
  IterateWindowListeners(window, &StopScreensharingCallback, nullptr);
}

// lets us do all sorts of things to the listeners
void
MediaManager::IterateWindowListeners(nsPIDOMWindow *aWindow,
                                     WindowListenerCallback aCallback,
                                     void *aData)
{
  // Iterate the docshell tree to find all the child windows, and for each
  // invoke the callback
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
  if (piWin) {
    if (piWin->IsInnerWindow() || piWin->GetCurrentInnerWindow()) {
      uint64_t windowID;
      if (piWin->IsInnerWindow()) {
        windowID = piWin->WindowID();
      } else {
        windowID = piWin->GetCurrentInnerWindow()->WindowID();
      }
      StreamListeners* listeners = GetActiveWindows()->Get(windowID);
      // pass listeners so it can modify/delete the list
      (*aCallback)(this, windowID, listeners, aData);
    }

    // iterate any children of *this* window (iframes, etc)
    nsCOMPtr<nsIDocShell> docShell = piWin->GetDocShell();
    if (docShell) {
      int32_t i, count;
      docShell->GetChildCount(&count);
      for (i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> item;
        docShell->GetChildAt(i, getter_AddRefs(item));
        nsCOMPtr<nsPIDOMWindow> win = item ? item->GetWindow() : nullptr;

        if (win) {
          IterateWindowListeners(win, aCallback, aData);
        }
      }
    }
  }
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

void
GetUserMediaCallbackMediaStreamListener::AudioConfig(bool aEchoOn,
              uint32_t aEcho,
              bool aAgcOn, uint32_t aAGC,
              bool aNoiseOn, uint32_t aNoise,
              int32_t aPlayoutDelay)
{
  if (mAudioSource) {
#ifdef MOZ_WEBRTC
    mMediaThread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(mAudioSource.get(), &MediaEngineSource::Config,
                        aEchoOn, aEcho, aAgcOn, aAGC, aNoiseOn,
                        aNoise, aPlayoutDelay));
#else
    unused << mMediaThread;
#endif
  }
}

// Can be invoked from EITHER MainThread or MSG thread
void
GetUserMediaCallbackMediaStreamListener::Invalidate()
{
  // We can't take a chance on blocking here, so proxy this to another
  // thread.
  // Pass a ref to us (which is threadsafe) so it can query us for the
  // source stream info.
  MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
    new MediaOperationTask(MEDIA_STOP,
                           this, nullptr, nullptr,
                           mAudioSource, mVideoSource,
                           mFinished, mWindowID, nullptr));
}

// Doesn't kill audio
// XXX refactor to combine with Invalidate()?
void
GetUserMediaCallbackMediaStreamListener::StopScreenWindowSharing()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  if (mVideoSource && !mStopped &&
      (mVideoSource->GetMediaSource() == MediaSourceType::Screen ||
       mVideoSource->GetMediaSource() == MediaSourceType::Application ||
       mVideoSource->GetMediaSource() == MediaSourceType::Window)) {
    // Stop the whole stream if there's no audio; just the video track if we have both
    MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
      new MediaOperationTask(mAudioSource ? MEDIA_STOP_TRACK : MEDIA_STOP,
                             this, nullptr, nullptr,
                             nullptr, mVideoSource,
                             mFinished, mWindowID, nullptr));
  }
}

// Stop backend for track

void
GetUserMediaCallbackMediaStreamListener::StopTrack(TrackID aID, bool aIsAudio)
{
  if (((aIsAudio && mAudioSource) ||
       (!aIsAudio && mVideoSource)) && !mStopped)
  {
    // XXX to support multiple tracks of a type in a stream, this should key off
    // the TrackID and not just the type
    MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
      new MediaOperationTask(MEDIA_STOP_TRACK,
                             this, nullptr, nullptr,
                             aIsAudio  ? mAudioSource : nullptr,
                             !aIsAudio ? mVideoSource : nullptr,
                             mFinished, mWindowID, nullptr));
  } else {
    LOG(("gUM track %d ended, but we don't have type %s",
         aID, aIsAudio ? "audio" : "video"));
  }
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
void
GetUserMediaCallbackMediaStreamListener::NotifyDirectListeners(MediaStreamGraph* aGraph,
                                                               bool aHasListeners)
{
  MediaManager::GetMessageLoop()->PostTask(FROM_HERE,
    new MediaOperationTask(MEDIA_DIRECT_LISTENERS,
                           this, nullptr, nullptr,
                           mAudioSource, mVideoSource,
                           aHasListeners, mWindowID, nullptr));
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
  case STOPPED_TRACK:
    msg = NS_LITERAL_STRING("shutdown");
    break;
  }

  nsCOMPtr<nsPIDOMWindow> window = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  return MediaManager::NotifyRecordingStatusChange(window, msg, mIsAudio, mIsVideo);
}

} // namespace mozilla
