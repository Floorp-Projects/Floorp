/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngine.h"
#include "mozilla/Services.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsObserverService.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "mozilla/Attributes.h"

namespace mozilla {

/**
 * This class is an implementation of MediaStreamListener. This is used
 * to Start() and Stop() the underlying MediaEngineSource when MediaStreams
 * are assigned and deassigned in content.
 */
class GetUserMediaCallbackMediaStreamListener : public MediaStreamListener
{
public:
  GetUserMediaCallbackMediaStreamListener(MediaEngineSource* aSource,
    nsDOMMediaStream* aStream, TrackID aListenId)
    : mSource(aSource)
    , mStream(aStream)
    , mID(aListenId)
    , mValid(true) {}

  void
  Invalidate()
  {
    if (!mValid) {
      return;
    }

    mValid = false;
    mSource->Stop();
    mSource->Deallocate();
  }

  void
  NotifyConsumptionChanged(MediaStreamGraph* aGraph, Consumption aConsuming)
  {
    if (aConsuming == CONSUMED) {
      SourceMediaStream* stream = mStream->GetStream()->AsSourceStream();
      mSource->Start(stream, mID);
      return;
    }

    // NOT_CONSUMED
    Invalidate();
    return;
  }

  void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked) {}
  void NotifyOutput(MediaStreamGraph* aGraph) {}
  void NotifyFinished(MediaStreamGraph* aGraph) {}
  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
    TrackRate aTrackRate, TrackTicks aTrackOffset,
    PRUint32 aTrackEvents, const MediaSegment& aQueuedMedia) {}

private:
  nsRefPtr<MediaEngineSource> mSource;
  nsCOMPtr<nsDOMMediaStream> mStream;
  TrackID mID;
  bool mValid;
};

typedef nsTArray<nsRefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

class MediaManager MOZ_FINAL : public nsIObserver {
public:
  static MediaManager* Get() {
    if (!sSingleton) {
      sSingleton = new MediaManager();

      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      obs->AddObserver(sSingleton, "xpcom-shutdown", false);
    }
    return sSingleton;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  MediaEngine* GetBackend();
  WindowTable* GetActiveWindows();

  nsresult GetUserMedia(nsPIDOMWindow* aWindow, nsIMediaStreamOptions* aParams,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);
  void OnNavigation(PRUint64 aWindowID);

private:
  // Make private because we want only one instance of this class
  MediaManager()
  : mBackend(nsnull)
  , mMediaThread(nsnull) {
    mActiveWindows.Init();
  };
  MediaManager(MediaManager const&) {};

  ~MediaManager() {
    delete mBackend;
  };

  MediaEngine* mBackend;
  nsCOMPtr<nsIThread> mMediaThread;

  WindowTable mActiveWindows;

  static nsRefPtr<MediaManager> sSingleton;
};

} // namespace mozilla
