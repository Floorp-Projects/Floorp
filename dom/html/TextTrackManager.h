/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackManager_h
#define mozilla_dom_TextTrackManager_h

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "TimeUnits.h"

class nsIWebVTTParserWrapper;

namespace mozilla {
namespace dom {

class HTMLMediaElement;

class CompareTextTracks {
 private:
  HTMLMediaElement* mMediaElement;
  int32_t TrackChildPosition(TextTrack* aTrack) const;

 public:
  explicit CompareTextTracks(HTMLMediaElement* aMediaElement);
  bool Equals(TextTrack* aOne, TextTrack* aTwo) const;
  bool LessThan(TextTrack* aOne, TextTrack* aTwo) const;
};

class TextTrack;
class TextTrackCue;

class TextTrackManager final : public nsIDOMEventListener {
  ~TextTrackManager();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TextTrackManager)

  NS_DECL_NSIDOMEVENTLISTENER

  explicit TextTrackManager(HTMLMediaElement* aMediaElement);

  TextTrackList* GetTextTracks() const;
  already_AddRefed<TextTrack> AddTextTrack(TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage,
                                           TextTrackMode aMode,
                                           TextTrackReadyState aReadyState,
                                           TextTrackSource aTextTrackSource);
  void AddTextTrack(TextTrack* aTextTrack);
  void RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly);
  void DidSeek();

  void NotifyCueAdded(TextTrackCue& aCue);
  void AddCues(TextTrack* aTextTrack);
  void NotifyCueRemoved(TextTrackCue& aCue);
  /**
   * Overview of WebVTT cuetext and anonymous content setup.
   *
   * WebVTT nodes are the parsed version of WebVTT cuetext. WebVTT cuetext is
   * the portion of a WebVTT cue that specifies what the caption will actually
   * show up as on screen.
   *
   * WebVTT cuetext can contain markup that loosely relates to HTML markup. It
   * can contain tags like <b>, <u>, <i>, <c>, <v>, <ruby>, <rt>, <lang>,
   * including timestamp tags.
   *
   * When the caption is ready to be displayed the WebVTT nodes are converted
   * over to anonymous DOM content. <i>, <u>, <b>, <ruby>, and <rt> all become
   * HTMLElements of their corresponding HTML markup tags. <c> and <v> are
   * converted to <span> tags. Timestamp tags are converted to XML processing
   * instructions. Additionally, all cuetext tags support specifying of classes.
   * This takes the form of <foo.class.subclass>. These classes are then parsed
   * and set as the anonymous content's class attribute.
   *
   * Rules on constructing DOM objects from WebVTT nodes can be found here
   * http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules.
   * Current rules are taken from revision on April 15, 2013.
   */

  void PopulatePendingList();

  void AddListeners();

  // The HTMLMediaElement that this TextTrackManager manages the TextTracks of.
  RefPtr<HTMLMediaElement> mMediaElement;

  void DispatchTimeMarchesOn();
  void TimeMarchesOn();
  void DispatchUpdateCueDisplay();

  void NotifyShutdown() { mShutdown = true; }

  void NotifyCueUpdated(TextTrackCue* aCue);

  void NotifyReset();

  bool IsLoaded();

 private:
  /**
   * Converts the TextTrackCue's cuetext into a tree of DOM objects
   * and attaches it to a div on its owning TrackElement's
   * MediaElement's caption overlay.
   */
  void UpdateCueDisplay();

  // List of the TextTrackManager's owning HTMLMediaElement's TextTracks.
  RefPtr<TextTrackList> mTextTracks;
  // List of text track objects awaiting loading.
  RefPtr<TextTrackList> mPendingTextTracks;
  // List of newly introduced Text Track cues.

  // Contain all cues for a MediaElement. Not sorted.
  RefPtr<TextTrackCueList> mNewCues;

  // True if the media player playback changed due to seeking prior to and
  // during running the "Time Marches On" algorithm.
  bool mHasSeeked;
  // Playback position at the time of last "Time Marches On" call
  media::TimeUnit mLastTimeMarchesOnCalled;

  bool mTimeMarchesOnDispatched;
  bool mUpdateCueDisplayDispatched;

  static StaticRefPtr<nsIWebVTTParserWrapper> sParserWrapper;

  bool performedTrackSelection;

  // Runs the algorithm for performing automatic track selection.
  void HonorUserPreferencesForTrackSelection();
  // Performs track selection for a single TextTrackKind.
  void PerformTrackSelection(TextTrackKind aTextTrackKind);
  // Performs track selection for a set of TextTrackKinds, for example,
  // 'subtitles' and 'captions' should be selected together.
  void PerformTrackSelection(TextTrackKind aTextTrackKinds[], uint32_t size);
  void GetTextTracksOfKinds(TextTrackKind aTextTrackKinds[], uint32_t size,
                            nsTArray<TextTrack*>& aTextTracks);
  void GetTextTracksOfKind(TextTrackKind aTextTrackKind,
                           nsTArray<TextTrack*>& aTextTracks);
  bool TrackIsDefault(TextTrack* aTextTrack);

  void ReportTelemetryForTrack(TextTrack* aTextTrack) const;
  void ReportTelemetryForCue();

  bool IsShutdown() const;

  // If there is at least one cue has been added to the cue list once, we would
  // report the usage of cue to Telemetry.
  bool mCueTelemetryReported;

  // This function will check media element's show poster flag to decide whether
  // we need to run `TimeMarchesOn`.
  void MaybeRunTimeMarchesOn();

  class ShutdownObserverProxy final : public nsIObserver {
    NS_DECL_ISUPPORTS

   public:
    explicit ShutdownObserverProxy(TextTrackManager* aManager)
        : mManager(aManager) {
      nsContentUtils::RegisterShutdownObserver(this);
    }

    NS_IMETHODIMP Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) override {
      MOZ_ASSERT(NS_IsMainThread());
      if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
        if (mManager) {
          mManager->NotifyShutdown();
        }
        Unregister();
      }
      return NS_OK;
    }

    void Unregister();

   private:
    ~ShutdownObserverProxy(){};
    TextTrackManager* mManager;
  };

  RefPtr<ShutdownObserverProxy> mShutdownProxy;
  bool mShutdown;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TextTrackManager_h
