/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "MediaStreamGraph.h"
#include "nsIDOMFile.h"
#include "nsIEventTarget.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPopupWindowManager.h"

#include "nsJSUtils.h"
#include "nsDOMFile.h"
#include "nsGlobalWindow.h"

/* Using WebRTC backend on Desktops (Mac, Windows, Linux), otherwise default */
#if defined(MOZ_WEBRTC)
#include "MediaEngineWebRTC.h"
#else
#include "MediaEngineDefault.h"
#endif

namespace mozilla {

/**
 * Send an error back to content. The error is the form a string.
 * Do this only on the main thread.
 */
class ErrorCallbackRunnable : public nsRunnable
{
public:
  ErrorCallbackRunnable(nsIDOMGetUserMediaErrorCallback* aError,
    const nsString& aErrorMsg, uint64_t aWindowID)
    : mError(aError)
    , mErrorMsg(aErrorMsg)
    , mWindowID(aWindowID) {}

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    WindowTable* activeWindows = MediaManager::Get()->GetActiveWindows();
    if (activeWindows->Get(mWindowID)) {
      mError->OnError(mErrorMsg);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  const nsString mErrorMsg;
  uint64_t mWindowID;
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
  SuccessCallbackRunnable(nsIDOMGetUserMediaSuccessCallback* aSuccess,
    nsIDOMFile* aFile, uint64_t aWindowID)
    : mSuccess(aSuccess)
    , mFile(aFile)
    , mWindowID(aWindowID) {}

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    WindowTable* activeWindows = MediaManager::Get()->GetActiveWindows();
    if (activeWindows->Get(mWindowID)) {
      // XPConnect is a magical unicorn.
      mSuccess->OnSuccess(mFile);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMFile> mFile;
  uint64_t mWindowID;
};

/**
 * Creates a MediaStream, attaches a listener and fires off a success callback
 * to the DOM with the stream.
 *
 * All of this must be done on the main thread!
 */
class GetUserMediaStreamRunnable : public nsRunnable
{
public:
  GetUserMediaStreamRunnable(nsIDOMGetUserMediaSuccessCallback* aSuccess,
    MediaEngineSource* aSource, StreamListeners* aListeners,
    uint64_t aWindowID, TrackID aTrackID)
    : mSuccess(aSuccess)
    , mSource(aSource)
    , mListeners(aListeners)
    , mWindowID(aWindowID)
    , mTrackID(aTrackID) {}

  ~GetUserMediaStreamRunnable() {}

  NS_IMETHOD
  Run()
  {
    // Create a media stream.
    nsCOMPtr<nsDOMMediaStream> stream = nsDOMMediaStream::CreateInputStream();

    nsPIDOMWindow *window = static_cast<nsPIDOMWindow*>
      (nsGlobalWindow::GetInnerWindowWithId(mWindowID));

    if (window && window->GetExtantDoc()) {
      stream->CombineWithPrincipal(window->GetExtantDoc()->NodePrincipal());
    }

    // Add our listener. We'll call Start() on the source when get a callback
    // that the MediaStream has started consuming. The listener is freed
    // when the page is invalidated (on navigation or close).
    GetUserMediaCallbackMediaStreamListener* listener =
      new GetUserMediaCallbackMediaStreamListener(mSource, stream, mTrackID);
    stream->GetStream()->AddListener(listener);

    // No need for locking because we always do this in the main thread.
    mListeners->AppendElement(listener);

    // We're in the main thread, so no worries here either.
    WindowTable* activeWindows = MediaManager::Get()->GetActiveWindows();
    if (activeWindows->Get(mWindowID)) {
      mSuccess->OnSuccess(stream);
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsRefPtr<MediaEngineSource> mSource;
  StreamListeners* mListeners;
  uint64_t mWindowID;
  TrackID mTrackID;
};

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
  GetUserMediaRunnable(bool aAudio, bool aVideo, bool aPicture,
    nsIDOMGetUserMediaSuccessCallback* aSuccess,
    nsIDOMGetUserMediaErrorCallback* aError,
    StreamListeners* aListeners, uint64_t aWindowID)
    : mAudio(aAudio)
    , mVideo(aVideo)
    , mPicture(aPicture)
    , mSuccess(aSuccess)
    , mError(aError)
    , mListeners(aListeners)
    , mWindowID(aWindowID) {}

  ~GetUserMediaRunnable() {}

  // We only support 1 audio and 1 video track for now.
  enum {
    kVideoTrack = 1,
    kAudioTrack = 2
  };

  NS_IMETHOD
  Run()
  {
    mManager = MediaManager::Get();

    // It is an error if audio or video are requested along with picture.
    if (mPicture && (mAudio || mVideo)) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NOT_SUPPORTED_ERR"), mWindowID
      ));
      return NS_OK;
    }

    if (mPicture) {
      SendPicture();
      return NS_OK;
    }

    // XXX: Implement merging two streams (See bug 758391).
    if (mAudio && mVideo) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NOT_IMPLEMENTED"), mWindowID
      ));
      return NS_OK;
    }

    if (mVideo) {
      SendVideo();
      return NS_OK;
    }

    if (mAudio) {
      SendAudio();
      return NS_OK;
    }

    return NS_OK;
  }

  /**
   * Allocates a video or audio device and returns a MediaStream via
   * a GetUserMediaStreamRunnable. Runs off the main thread.
   */
  void
  ProcessGetUserMedia(MediaEngineSource* aSource, TrackID aTrackID)
  {
    /**
     * Normally we would now get the name & UUID for the device and ask the
     * user permission. We will do that when we have some UI. Currently,
     * only the Android {picture:true} backend is functional, which does not
     * need a permission prompt, as permission is implicit by user action.
     *
     * See bug 748835 for progress on the desktop UI.
     */
    nsresult rv = aSource->Allocate();
    if (NS_FAILED(rv)) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
      ));
      return;
    }

    NS_DispatchToMainThread(new GetUserMediaStreamRunnable(
      mSuccess.get(), aSource, mListeners, mWindowID, aTrackID
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
    nsresult rv = aSource->Allocate();
    if (NS_FAILED(rv)) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
      ));
      return;
    }

    nsCOMPtr<nsIDOMFile> file;
    aSource->Snapshot(aDuration, getter_AddRefs(file));
    aSource->Deallocate();

    NS_DispatchToMainThread(new SuccessCallbackRunnable(
      mSuccess, file, mWindowID
    ));
    return;
  }

  // {picture:true}
  void
  SendPicture()
  {
    nsTArray<nsRefPtr<MediaEngineVideoSource> > videoSources;
    mManager->GetBackend()->EnumerateVideoDevices(&videoSources);

    uint32_t count = videoSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }

    // We pick the first source as the "default". Work is needed here in the
    // form of UI to let the user pick a source. (Also true for audio).
    MediaEngineVideoSource* videoSource = videoSources[0];
    ProcessGetUserMediaSnapshot(videoSource, 0 /* duration */);
  }

  // {video:true}
  void
  SendVideo()
  {
    nsTArray<nsRefPtr<MediaEngineVideoSource> > videoSources;
    mManager->GetBackend()->EnumerateVideoDevices(&videoSources);

    uint32_t count = videoSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }

    MediaEngineVideoSource* videoSource = videoSources[0];
    ProcessGetUserMedia(videoSource, kVideoTrack);
  }

  // {audio:true}
  void
  SendAudio()
  {
    nsTArray<nsRefPtr<MediaEngineAudioSource> > audioSources;
    mManager->GetBackend()->EnumerateAudioDevices(&audioSources);

    uint32_t count = audioSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }

    MediaEngineAudioSource* audioSource = audioSources[0];
    ProcessGetUserMedia(audioSource, kAudioTrack);
  }

private:
  bool mAudio;
  bool mVideo;
  bool mPicture;

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  StreamListeners* mListeners;
  uint64_t mWindowID;

  MediaManager* mManager;
};


nsRefPtr<MediaManager> MediaManager::sSingleton;

NS_IMPL_ISUPPORTS1(MediaManager, nsIObserver)

/**
 * The entry point for this file. A call from Navigator::mozGetUserMedia
 * will end up here. MediaManager is a singleton that is responsible
 * for handling all incoming getUserMedia calls from every window.
 */
nsresult
MediaManager::GetUserMedia(nsPIDOMWindow* aWindow, nsIMediaStreamOptions* aParams,
  nsIDOMGetUserMediaSuccessCallback* onSuccess,
  nsIDOMGetUserMediaErrorCallback* onError)
{
  NS_ENSURE_TRUE(aParams, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aWindow, NS_ERROR_NULL_POINTER);

  bool audio, video, picture;

  nsresult rv = aParams->GetPicture(&picture);
  NS_ENSURE_SUCCESS(rv, rv);

  /**
   * If we were asked to get a picture, before getting a snapshot, we check if
   * the calling page is allowed to open a popup. We do this because
   * {picture:true} will open a new "window" to let the user preview or select
   * an image, on Android. The desktop UI for {picture:true} is TBD, at which
   * may point we can decide whether to extend this test there as well.
   */
#if !defined(MOZ_WEBRTC)
  if (picture) {
    if (aWindow->GetPopupControlState() > openControlled) {
      nsCOMPtr<nsIPopupWindowManager> pm =
        do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);
      if (!pm)
        return NS_OK;

      uint32_t permission;
      nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
      pm->TestPermission(doc->NodePrincipal(), &permission);
      if ((permission == nsIPopupWindowManager::DENY_POPUP)) {
        nsCOMPtr<nsIDOMDocument> domDoc = aWindow->GetExtantDocument();
        nsGlobalWindow::FirePopupBlockedEvent(
          domDoc, aWindow, nullptr, EmptyString(), EmptyString()
                                              );
        return NS_OK;
      }
    }
  }
#endif

  rv = aParams->GetAudio(&audio);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aParams->GetVideo(&video);
  NS_ENSURE_SUCCESS(rv, rv);

  // We only support "front" or "back". TBD: Send to GetUserMediaRunnable.
  nsString cameraType;
  rv = aParams->GetCamera(cameraType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the WindowID in a hash table and mark as active. The entry is removed
  // when this window is closed or navigated away from.
  uint64_t windowID = aWindow->WindowID();
  StreamListeners* listeners = mActiveWindows.Get(windowID);
  if (!listeners) {
    listeners = new StreamListeners;
    mActiveWindows.Put(windowID, listeners);
  }

  // Pass runnables along to GetUserMediaRunnable so it can add the
  // MediaStreamListener to the runnable list.
  nsCOMPtr<nsIRunnable> gUMRunnable = new GetUserMediaRunnable(
    audio, video, picture, onSuccess, onError, listeners, windowID
  );

  if (picture) {
    // ShowFilePickerForMimeType() must run on the Main Thread! (on Android)
    NS_DispatchToMainThread(gUMRunnable);
  } else {
    // Reuse the same thread to save memory.
    if (!mMediaThread) {
      rv = NS_NewThread(getter_AddRefs(mMediaThread));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mMediaThread->Dispatch(gUMRunnable, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

MediaEngine*
MediaManager::GetBackend()
{
  // Plugin backends as appropriate. The default engine also currently
  // includes picture support for Android.
  if (!mBackend) {
#if defined(MOZ_WEBRTC)
    mBackend = new MediaEngineWebRTC();
#else
    mBackend = new MediaEngineDefault();
#endif
  }

  return mBackend;
}

WindowTable*
MediaManager::GetActiveWindows()
{
  return &mActiveWindows;
}

void
MediaManager::OnNavigation(uint64_t aWindowID)
{
  // Invalidate this window. The runnables check this value before making
  // a call to content.
  StreamListeners* listeners = mActiveWindows.Get(aWindowID);
  if (!listeners) {
    return;
  }

  uint32_t length = listeners->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsRefPtr<GetUserMediaCallbackMediaStreamListener> listener =
      listeners->ElementAt(i);
    listener->Invalidate();
    listener = nullptr;
  }
  listeners->Clear();

  mActiveWindows.Remove(aWindowID);
}

nsresult
MediaManager::Observe(nsISupports* aSubject, const char* aTopic,
  const PRUnichar* aData)
{
  if (strcmp(aTopic, "xpcom-shutdown")) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->RemoveObserver(this, "xpcom-shutdown");

  // Close off any remaining active windows.
  mActiveWindows.Clear();
  sSingleton = nullptr;

  return NS_OK;
}

} // namespace mozilla
