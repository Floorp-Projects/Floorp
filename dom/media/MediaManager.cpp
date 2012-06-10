/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "MediaStreamGraph.h"
#include "MediaEngineDefault.h"

#include "nsIDOMFile.h"
#include "nsIEventTarget.h"
#include "nsIScriptGlobalObject.h"

#include "nsJSUtils.h"
#include "nsDOMFile.h"

namespace mozilla {

/**
 * Send an error back to content. The error is the form a string.
 * Do this only on the main thread.
 */
class ErrorCallbackRunnable : public nsRunnable
{
public:
  ErrorCallbackRunnable(nsIDOMGetUserMediaErrorCallback* aError,
    const nsString& aErrorMsg, PRUint64 aWindowID)
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
  PRUint64 mWindowID;
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
    nsIDOMFile* aFile, PRUint64 aWindowID)
    : mSuccess(aSuccess)
    , mFile(aFile)
    , mWindowID(aWindowID) {}

  SuccessCallbackRunnable(nsIDOMGetUserMediaSuccessCallback* aSuccess,
    nsIDOMMediaStream* aStream, PRUint64 aWindowID)
    : mSuccess(aSuccess)
    , mStream(aStream)
    , mWindowID(aWindowID) {}

  NS_IMETHOD
  Run()
  {
    // Only run if the window is still active.
    WindowTable* activeWindows = MediaManager::Get()->GetActiveWindows();
    if (activeWindows->Get(mWindowID)) {
      // XPConnect is a magical unicorn.
      if (mFile) {
        mSuccess->OnSuccess(mFile);
      } else if (mStream) {
        mSuccess->OnSuccess(mStream);
      }
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMFile> mFile;
  nsCOMPtr<nsIDOMMediaStream> mStream;
  PRUint64 mWindowID;
};

/**
 * This runnable creates a nsDOMMediaStream from a given MediaEngineSource
 * and returns it via a success callback. Both must be done on the main thread.
 */
class GetUserMediaCallbackRunnable : public nsRunnable
{
public:
  GetUserMediaCallbackRunnable(MediaEngineSource* aSource, TrackID aId,
    nsIDOMGetUserMediaSuccessCallback* aSuccess,
    nsIDOMGetUserMediaErrorCallback* aError,
    PRUint64 aWindowID,
    StreamListeners* aListeners)
    : mSource(aSource)
    , mId(aId)
    , mSuccess(aSuccess)
    , mError(aError)
    , mWindowID(aWindowID)
    , mListeners(aListeners) {}

  NS_IMETHOD
  Run()
  {
    /**
     * Normally we would now get the name & UUID for the device and ask the
     * user permission. We will do that when we have some UI. Currently,
     * only the Android {picture:true} backend is functional, which does not
     * need a permission prompt, as permission is implicit by user action.
     *
     * See bug 748835 for progress on the desktop UI.
     */
    nsCOMPtr<nsDOMMediaStream> comStream = mSource->Allocate();
    if (!comStream) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
      ));
      return NS_OK;
    }

    // Add our listener. We'll call Start() on the source when get a callback
    // that the MediaStream has started consuming. The listener is freed
    // when the page is invalidated (on navigation or close).
    GetUserMediaCallbackMediaStreamListener* listener =
      new GetUserMediaCallbackMediaStreamListener(mSource, comStream, mId);
    comStream->GetStream()->AddListener(listener);

    {
      MutexAutoLock lock(*(MediaManager::Get()->GetLock()));
      mListeners->AppendElement(listener);
    }

    // Add the listener to CallbackRunnables so it can be invalidated.
    NS_DispatchToMainThread(new SuccessCallbackRunnable(
      mSuccess, comStream.get(), mWindowID
    ));
    return NS_OK;
  }

private:
  nsCOMPtr<MediaEngineSource> mSource;
  TrackID mId;
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  PRUint64 mWindowID;
  StreamListeners* mListeners;
};

/**
 * This runnable creates a nsIDOMFile from a MediaEngineVideoSource and
 * passes the result back via a SuccessRunnable. Both must be done on the
 * main thread.
 */
class GetUserMediaSnapshotCallbackRunable : public nsRunnable
{
public:
  GetUserMediaSnapshotCallbackRunable(MediaEngineSource* aSource,
    PRUint32 aDuration,
    nsIDOMGetUserMediaSuccessCallback* aSuccessCallback,
    nsIDOMGetUserMediaErrorCallback* aErrorCallback,
    PRUint64 aWindowID)
    : mSource(aSource)
    , mDuration(aDuration)
    , mSuccessCallback(aSuccessCallback)
    , mErrorCallback(aErrorCallback)
    , mWindowID(aWindowID) {}

  NS_IMETHOD
  Run()
  {
    nsCOMPtr<nsDOMMediaStream> comStream = mSource->Allocate();
    if (!comStream) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mErrorCallback, NS_LITERAL_STRING("HARDWARE_UNAVAILABLE"), mWindowID
      ));
      return NS_OK;
    }

    nsCOMPtr<nsIDOMFile> file;
    mSource->Snapshot(mDuration, getter_AddRefs(file));
    mSource->Deallocate();

    NS_DispatchToMainThread(new SuccessCallbackRunnable(
      mSuccessCallback, file, mWindowID
    ));
    return NS_OK;
  }

private:
  nsCOMPtr<MediaEngineSource> mSource;
  PRUint32 mDuration;
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccessCallback;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback>  mErrorCallback;
  PRUint64 mWindowID;
};

/**
 * Runs on a seperate thread and is responsible for enumerating devices.
 * Depending on whether a picture or stream was asked for, either
 * GetUserMediaCallbackRunnable or GetUserMediaSnapshotCallbackRunnable
 * will be dispatched to the main thread to return the result to DOM.
 *
 * Do not run this on the main thread.
 */
class GetUserMediaRunnable : public nsRunnable
{
public:
  GetUserMediaRunnable(bool aAudio, bool aVideo, bool aPicture,
    nsIDOMGetUserMediaSuccessCallback* aSuccess,
    nsIDOMGetUserMediaErrorCallback* aError,
    PRUint64 aWindowID, StreamListeners* aListeners)
    : mAudio(aAudio)
    , mVideo(aVideo)
    , mPicture(aPicture)
    , mSuccess(aSuccess)
    , mError(aError)
    , mWindowID(aWindowID)
    , mListeners(aListeners) {}

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

  // {picture:true}
  void
  SendPicture()
  {
    nsTArray<nsRefPtr<MediaEngineVideoSource> > videoSources;
    mManager->GetBackend()->EnumerateVideoDevices(&videoSources);

    PRUint32 count = videoSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }
    MediaEngineVideoSource* videoSource = videoSources[count - 1];
    NS_DispatchToMainThread(new GetUserMediaSnapshotCallbackRunable(
      videoSource, 0 /* duration */, mSuccess, mError, mWindowID
    ));
  }

  // {video:true}
  void
  SendVideo()
  {
    nsTArray<nsRefPtr<MediaEngineVideoSource> > videoSources;
    mManager->GetBackend()->EnumerateVideoDevices(&videoSources);

    PRUint32 count = videoSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }

    MediaEngineVideoSource* videoSource = videoSources[count - 1];
    NS_DispatchToMainThread(new GetUserMediaCallbackRunnable(
      videoSource, kVideoTrack, mSuccess, mError, mWindowID, mListeners
    ));
  }

  // {audio:true}
  void
  SendAudio()
  {
    nsTArray<nsRefPtr<MediaEngineAudioSource> > audioSources;
    mManager->GetBackend()->EnumerateAudioDevices(&audioSources);

    PRUint32 count = audioSources.Length();
    if (!count) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(
        mError, NS_LITERAL_STRING("NO_DEVICES_FOUND"), mWindowID
      ));
      return;
    }

    MediaEngineAudioSource* audioSource = audioSources[count - 1];
    NS_DispatchToMainThread(new GetUserMediaCallbackRunnable(
      audioSource, kAudioTrack, mSuccess, mError, mWindowID, mListeners
    ));
  }

private:
  bool mAudio;
  bool mVideo;
  bool mPicture;

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  PRUint64 mWindowID;
  StreamListeners* mListeners;

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
MediaManager::GetUserMedia(PRUint64 aWindowID, nsIMediaStreamOptions* aParams,
  nsIDOMGetUserMediaSuccessCallback* onSuccess,
  nsIDOMGetUserMediaErrorCallback* onError)
{
  NS_ENSURE_TRUE(aParams, NS_ERROR_NULL_POINTER);

  bool audio, video, picture;
  nsresult rv = aParams->GetPicture(&picture);
  NS_ENSURE_SUCCESS(rv, rv);

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
  StreamListeners* listeners = mActiveWindows.Get(aWindowID);
  if (!listeners) {
    listeners = new StreamListeners;
    mActiveWindows.Put(aWindowID, listeners);
  }

  // Pass runanbles along to GetUserMediaRunnable so it can add the
  // MediaStreamListener to the runnable list.
  nsCOMPtr<nsIRunnable> gUMRunnable = new GetUserMediaRunnable(
    audio, video, picture, onSuccess, onError, aWindowID, listeners
  );

  // Reuse the same thread to save memory.
  if (!mMediaThread) {
    rv = NS_NewThread(getter_AddRefs(mMediaThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mMediaThread->Dispatch(gUMRunnable, NS_DISPATCH_NORMAL);
  return NS_OK;
}

MediaEngine*
MediaManager::GetBackend()
{
  // Plugin backends as appropriate. Only default is available for now, which
  // also includes picture support for Android.
  if (!mBackend) {
    mBackend = new MediaEngineDefault();
  }
  return mBackend;
}

WindowTable*
MediaManager::GetActiveWindows()
{
  return &mActiveWindows;
}

void
MediaManager::OnNavigation(PRUint64 aWindowID)
{
  // Invalidate this window. The runnables check this value before making
  // a call to content.
  StreamListeners* listeners = mActiveWindows.Get(aWindowID);
  if (!listeners) {
    return;
  }

  MutexAutoLock lock(*mLock);
  PRUint32 length = listeners->Length();
  for (PRUint32 i = 0; i < length; i++) {
    nsRefPtr<GetUserMediaCallbackMediaStreamListener> listener =
      listeners->ElementAt(i);
    listener->Invalidate();
    listener = nsnull;
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
  sSingleton = nsnull;

  return NS_OK;
}

} // namespace mozilla
