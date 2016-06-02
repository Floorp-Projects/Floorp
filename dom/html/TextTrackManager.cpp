/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/Event.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsComponentManagerUtils.h"
#include "nsVariant.h"
#include "nsVideoFrame.h"
#include "nsIFrame.h"
#include "nsTArrayHelpers.h"
#include "nsIWebVTTParserWrapper.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TextTrackManager::ShutdownObserverProxy, nsIObserver);

CompareTextTracks::CompareTextTracks(HTMLMediaElement* aMediaElement)
{
  mMediaElement = aMediaElement;
}

int32_t
CompareTextTracks::TrackChildPosition(TextTrack* aTextTrack) const {
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return -1;
  }
  return mMediaElement->IndexOf(trackElement);
}

bool
CompareTextTracks::Equals(TextTrack* aOne, TextTrack* aTwo) const {
  // Two tracks can never be equal. If they have corresponding TrackElements
  // they would need to occupy the same tree position (impossible) and in the
  // case of tracks coming from AddTextTrack source we put the newest at the
  // last position, so they won't be equal as well.
  return false;
}

bool
CompareTextTracks::LessThan(TextTrack* aOne, TextTrack* aTwo) const
{
  TextTrackSource sourceOne = aOne->GetTextTrackSource();
  TextTrackSource sourceTwo = aTwo->GetTextTrackSource();
  if (sourceOne != sourceTwo) {
    return sourceOne == Track ||
           (sourceOne == AddTextTrack && sourceTwo == MediaResourceSpecific);
  }
  switch (sourceOne) {
    case Track: {
      int32_t positionOne = TrackChildPosition(aOne);
      int32_t positionTwo = TrackChildPosition(aTwo);
      // If either position one or positiontwo are -1 then something has gone
      // wrong. In this case we should just put them at the back of the list.
      return positionOne != -1 && positionTwo != -1 &&
             positionOne < positionTwo;
    }
    case AddTextTrack:
      // For AddTextTrack sources the tracks will already be in the correct relative
      // order in the source array. Assume we're called in iteration order and can
      // therefore always report aOne < aTwo to maintain the original temporal ordering.
      return true;
    case MediaResourceSpecific:
      // No rules for Media Resource Specific tracks yet.
      break;
  }
  return true;
}

NS_IMPL_CYCLE_COLLECTION(TextTrackManager, mMediaElement, mTextTracks,
                         mPendingTextTracks, mNewCues,
                         mLastActiveCues)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackManager)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackManager::sParserWrapper;

TextTrackManager::TextTrackManager(HTMLMediaElement *aMediaElement)
  : mMediaElement(aMediaElement)
  , mHasSeeked(false)
  , mLastTimeMarchesOnCalled(0.0)
  , mTimeMarchesOnDispatched(false)
  , performedTrackSelection(false)
  , mShutdown(false)
{
  nsISupports* parentObject =
    mMediaElement->OwnerDoc()->GetParentObject();

  NS_ENSURE_TRUE_VOID(parentObject);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);
  mNewCues = new TextTrackCueList(window);
  mLastActiveCues = new TextTrackCueList(window);
  mTextTracks = new TextTrackList(window, this);
  mPendingTextTracks = new TextTrackList(window, this);

  if (!sParserWrapper) {
    nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
      do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID);
    sParserWrapper = parserWrapper;
    ClearOnShutdown(&sParserWrapper);
  }
  mShutdownProxy = new ShutdownObserverProxy(this);
}

TextTrackManager::~TextTrackManager()
{
  nsContentUtils::UnregisterShutdownObserver(mShutdownProxy);
}

TextTrackList*
TextTrackManager::GetTextTracks() const
{
  return mTextTracks;
}

already_AddRefed<TextTrack>
TextTrackManager::AddTextTrack(TextTrackKind aKind, const nsAString& aLabel,
                               const nsAString& aLanguage,
                               TextTrackMode aMode,
                               TextTrackReadyState aReadyState,
                               TextTrackSource aTextTrackSource)
{
  if (!mMediaElement || !mTextTracks) {
    return nullptr;
  }
  RefPtr<TextTrack> ttrack =
    mTextTracks->AddTextTrack(aKind, aLabel, aLanguage, aMode, aReadyState,
                              aTextTrackSource, CompareTextTracks(mMediaElement));
  AddCues(ttrack);

  if (aTextTrackSource == Track) {
    HonorUserPreferencesForTrackSelection();
  }

  return ttrack.forget();
}

void
TextTrackManager::AddTextTrack(TextTrack* aTextTrack)
{
  if (!mMediaElement || !mTextTracks) {
    return;
  }
  mTextTracks->AddTextTrack(aTextTrack, CompareTextTracks(mMediaElement));
  AddCues(aTextTrack);
  if (aTextTrack->GetTextTrackSource() == Track) {
    HonorUserPreferencesForTrackSelection();
  }
}

void
TextTrackManager::AddCues(TextTrack* aTextTrack)
{
  if (!mNewCues) {
    return;
  }

  TextTrackCueList* cueList = aTextTrack->GetCues();
  if (cueList) {
    bool dummy;
    for (uint32_t i = 0; i < cueList->Length(); ++i) {
      mNewCues->AddCue(*cueList->IndexedGetter(i, dummy));
    }
    DispatchTimeMarchesOn();
  }
}

void
TextTrackManager::RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly)
{
  if (!mPendingTextTracks || !mTextTracks) {
    return;
  }

  mPendingTextTracks->RemoveTextTrack(aTextTrack);
  if (aPendingListOnly) {
    return;
  }

  mTextTracks->RemoveTextTrack(aTextTrack);
  // Remove the cues in mNewCues belong to aTextTrack.
  TextTrackCueList* removeCueList = aTextTrack->GetCues();
  if (removeCueList) {
    for (uint32_t i = 0; i < removeCueList->Length(); ++i) {
      mNewCues->RemoveCue(*((*removeCueList)[i]));
    }
    DispatchTimeMarchesOn();
  }
}

void
TextTrackManager::DidSeek()
{
  if (mTextTracks) {
    mTextTracks->DidSeek();
  }
  if (mMediaElement) {
    mLastTimeMarchesOnCalled = mMediaElement->CurrentTime();
  }
  mHasSeeked = true;
}

void
TextTrackManager::UpdateCueDisplay()
{
  if (!mMediaElement || !mTextTracks) {
    return;
  }

  nsIFrame* frame = mMediaElement->GetPrimaryFrame();
  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    return;
  }

  nsCOMPtr<nsIContent> overlay = videoFrame->GetCaptionOverlay();
  if (!overlay) {
    return;
  }

  nsTArray<RefPtr<TextTrackCue> > activeCues;
  mTextTracks->UpdateAndGetShowingCues(activeCues);

  if (activeCues.Length() > 0) {
    RefPtr<nsVariantCC> jsCues = new nsVariantCC();

    jsCues->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                       &NS_GET_IID(nsIDOMEventTarget),
                       activeCues.Length(),
                       static_cast<void*>(activeCues.Elements()));

    nsPIDOMWindowInner* window = mMediaElement->OwnerDoc()->GetInnerWindow();
    if (window) {
      sParserWrapper->ProcessCues(window, jsCues, overlay);
    }
  } else if (overlay->Length() > 0) {
    nsContentUtils::SetNodeTextContent(overlay, EmptyString(), true);
  }
}

void
TextTrackManager::NotifyCueAdded(TextTrackCue& aCue)
{
  if (mNewCues) {
    mNewCues->AddCue(aCue);
  }
  DispatchTimeMarchesOn();
}

void
TextTrackManager::NotifyCueRemoved(TextTrackCue& aCue)
{
  if (mNewCues) {
    mNewCues->RemoveCue(aCue);
  }
  DispatchTimeMarchesOn();
}

void
TextTrackManager::PopulatePendingList()
{
  if (!mTextTracks || !mPendingTextTracks || !mMediaElement) {
    return;
  }
  uint32_t len = mTextTracks->Length();
  bool dummy;
  for (uint32_t index = 0; index < len; ++index) {
    TextTrack* ttrack = mTextTracks->IndexedGetter(index, dummy);
    if (ttrack && ttrack->Mode() != TextTrackMode::Disabled &&
        ttrack->ReadyState() == TextTrackReadyState::Loading) {
      mPendingTextTracks->AddTextTrack(ttrack,
                                       CompareTextTracks(mMediaElement));
    }
  }
}

void
TextTrackManager::AddListeners()
{
  if (mMediaElement) {
    mMediaElement->AddEventListener(NS_LITERAL_STRING("resizevideocontrols"),
                                    this, false, false);
  }
}

void
TextTrackManager::HonorUserPreferencesForTrackSelection()
{
  if (performedTrackSelection || !mTextTracks) {
    return;
  }

  TextTrackKind ttKinds[] = { TextTrackKind::Captions,
                              TextTrackKind::Subtitles };

  // Steps 1 - 3: Perform automatic track selection for different TextTrack
  // Kinds.
  PerformTrackSelection(ttKinds, ArrayLength(ttKinds));
  PerformTrackSelection(TextTrackKind::Descriptions);
  PerformTrackSelection(TextTrackKind::Chapters);

  // Step 4: Set all TextTracks with a kind of metadata that are disabled
  // to hidden.
  for (uint32_t i = 0; i < mTextTracks->Length(); i++) {
    TextTrack* track = (*mTextTracks)[i];
    if (track->Kind() == TextTrackKind::Metadata && TrackIsDefault(track) &&
        track->Mode() == TextTrackMode::Disabled) {
      track->SetMode(TextTrackMode::Hidden);
    }
  }

  performedTrackSelection = true;
}

bool
TextTrackManager::TrackIsDefault(TextTrack* aTextTrack)
{
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return false;
  }
  return trackElement->Default();
}

void
TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKind)
{
  TextTrackKind ttKinds[] = { aTextTrackKind };
  PerformTrackSelection(ttKinds, ArrayLength(ttKinds));
}

void
TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKinds[],
                                        uint32_t size)
{
  nsTArray<TextTrack*> candidates;
  GetTextTracksOfKinds(aTextTrackKinds, size, candidates);

  // Step 3: If any TextTracks in candidates are showing then abort these steps.
  for (uint32_t i = 0; i < candidates.Length(); i++) {
    if (candidates[i]->Mode() == TextTrackMode::Showing) {
      return;
    }
  }

  // Step 4: Honor user preferences for track selection, otherwise, set the
  // first TextTrack in candidates with a default attribute to showing.
  // TODO: Bug 981691 - Honor user preferences for text track selection.
  for (uint32_t i = 0; i < candidates.Length(); i++) {
    if (TrackIsDefault(candidates[i]) &&
        candidates[i]->Mode() == TextTrackMode::Disabled) {
      candidates[i]->SetMode(TextTrackMode::Showing);
      return;
    }
  }
}

void
TextTrackManager::GetTextTracksOfKinds(TextTrackKind aTextTrackKinds[],
                                       uint32_t size,
                                       nsTArray<TextTrack*>& aTextTracks)
{
  for (uint32_t i = 0; i < size; i++) {
    GetTextTracksOfKind(aTextTrackKinds[i], aTextTracks);
  }
}

void
TextTrackManager::GetTextTracksOfKind(TextTrackKind aTextTrackKind,
                                      nsTArray<TextTrack*>& aTextTracks)
{
  if (!mTextTracks) {
    return;
  }
  for (uint32_t i = 0; i < mTextTracks->Length(); i++) {
    TextTrack* textTrack = (*mTextTracks)[i];
    if (textTrack->Kind() == aTextTrackKind) {
      aTextTracks.AppendElement(textTrack);
    }
  }
}

NS_IMETHODIMP
TextTrackManager::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mTextTracks) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("resizevideocontrols")) {
    for (uint32_t i = 0; i< mTextTracks->Length(); i++) {
      ((*mTextTracks)[i])->SetCuesDirty();
    }
  }
  return NS_OK;
}


class SimpleTextTrackEvent : public Runnable
{
public:
  friend class CompareSimpleTextTrackEvents;
  SimpleTextTrackEvent(const nsAString& aEventName, double aTime,
                       TextTrack* aTrack, TextTrackCue* aCue)
  : mName(aEventName),
    mTime(aTime),
    mTrack(aTrack),
    mCue(aCue)
  {}

  NS_IMETHOD Run() {
    mCue->DispatchTrustedEvent(mName);
    return NS_OK;
  }

private:
  nsString mName;
  double mTime;
  TextTrack* mTrack;
  RefPtr<TextTrackCue> mCue;
};

class CompareSimpleTextTrackEvents {
private:
  int32_t TrackChildPosition(SimpleTextTrackEvent* aEvent) const
  {
    HTMLTrackElement* trackElement = aEvent->mTrack->GetTrackElement();;
    if (!trackElement) {
      return -1;
    }
    return mMediaElement->IndexOf(trackElement);
  }
  HTMLMediaElement* mMediaElement;
public:
  explicit CompareSimpleTextTrackEvents(HTMLMediaElement* aMediaElement)
  {
    mMediaElement = aMediaElement;
  }

  bool Equals(SimpleTextTrackEvent* aOne, SimpleTextTrackEvent* aTwo) const
  {
    return false;
  }

  bool LessThan(SimpleTextTrackEvent* aOne, SimpleTextTrackEvent* aTwo) const
  {
    if (aOne->mTime < aTwo->mTime) {
      return true;
    } else if (aOne->mTime > aTwo->mTime) {
      return false;
    }

    int32_t positionOne = TrackChildPosition(aOne);
    int32_t positionTwo = TrackChildPosition(aTwo);
    if (positionOne < positionTwo) {
      return true;
    } else if (positionOne > positionTwo) {
      return false;
    }

    if (aOne->mName.EqualsLiteral("enter") ||
        aTwo->mName.EqualsLiteral("exit")) {
      return true;
    }
    return false;
  }
};

class TextTrackListInternal
{
public:
  void AddTextTrack(TextTrack* aTextTrack,
                    const CompareTextTracks& aCompareTT)
  {
    if (!mTextTracks.Contains(aTextTrack)) {
      mTextTracks.InsertElementSorted(aTextTrack, aCompareTT);
    }
  }
  uint32_t Length() const
  {
    return mTextTracks.Length();
  }
  TextTrack* operator[](uint32_t aIndex)
  {
    return mTextTracks.SafeElementAt(aIndex, nullptr);
  }
private:
  nsTArray<RefPtr<TextTrack>> mTextTracks;
};

void
TextTrackManager::DispatchTimeMarchesOn()
{
  // Run the algorithm if no previous instance is still running, otherwise
  // enqueue the current playback position and whether only that changed
  // through its usual monotonic increase during normal playback; current
  // executing call upon completion will check queue for further 'work'.
  if (!mTimeMarchesOnDispatched && !mShutdown) {
    NS_DispatchToMainThread(NewRunnableMethod(this, &TextTrackManager::TimeMarchesOn));
    mTimeMarchesOnDispatched = true;
  }
}

// https://html.spec.whatwg.org/multipage/embedded-content.html#time-marches-on
void
TextTrackManager::TimeMarchesOn()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mTimeMarchesOnDispatched = false;

  nsISupports* parentObject =
    mMediaElement->OwnerDoc()->GetParentObject();
  if (NS_WARN_IF(!parentObject)) {
    return;
  }
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);

  if (mMediaElement &&
      (!(mMediaElement->GetPlayedOrSeeked()) || mMediaElement->Seeking())) {
    return;
  }

  // Step 3.
  double currentPlaybackTime = mMediaElement->CurrentTime();
  bool hasNormalPlayback = !mHasSeeked;
  mHasSeeked = false;

  // Step 1, 2.
  RefPtr<TextTrackCueList> currentCues =
    new TextTrackCueList(window);
  RefPtr<TextTrackCueList> otherCues =
    new TextTrackCueList(window);
  bool dummy;
  for (uint32_t index = 0; index < mTextTracks->Length(); ++index) {
    TextTrack* ttrack = mTextTracks->IndexedGetter(index, dummy);
    if (ttrack && dummy) {
      // TODO: call GetCueListByTimeInterval on mNewCues?
      TextTrackCueList* activeCueList = ttrack->GetActiveCues();
      if (activeCueList) {
        for (uint32_t i = 0; i < activeCueList->Length(); ++i) {
          currentCues->AddCue(*((*activeCueList)[i]));
        }
      }
    }
  }
  // Populate otherCues with 'non-active" cues.
  if (hasNormalPlayback) {
    media::Interval<double> interval(mLastTimeMarchesOnCalled,
                                     currentPlaybackTime);
    otherCues = mNewCues->GetCueListByTimeInterval(interval);;
  } else {
    // Seek case. Put the mLastActiveCues into otherCues.
    otherCues = mLastActiveCues;
  }
  for (uint32_t i = 0; i < currentCues->Length(); ++i) {
    TextTrackCue* cue = (*currentCues)[i];
    otherCues->RemoveCue(*cue);
  }

  // Step 4.
  RefPtr<TextTrackCueList> missedCues = new TextTrackCueList(window);
  if (hasNormalPlayback) {
    for (uint32_t i = 0; i < otherCues->Length(); ++i) {
      TextTrackCue* cue = (*otherCues)[i];
      if (cue->StartTime() >= mLastTimeMarchesOnCalled &&
          cue->EndTime() <= currentPlaybackTime) {
        missedCues->AddCue(*cue);
      }
    }
  }

  // Step 5. Empty now.
  // TODO: Step 6: fire timeupdate?

  // Step 7. Abort steps if condition 1, 2, 3 are satisfied.
  // 1. All of the cues in current cues have their active flag set.
  // 2. None of the cues in other cues have their active flag set.
  // 3. Missed cues is empty.
  bool c1 = true;
  for (uint32_t i = 0; i < currentCues->Length(); ++i) {
    if (!(*currentCues)[i]->GetActive()) {
      c1 = false;
      break;
    }
  }
  bool c2 = true;
  for (uint32_t i = 0; i < otherCues->Length(); ++i) {
    if ((*otherCues)[i]->GetActive()) {
      c2 = false;
      break;
    }
  }
  bool c3 = (missedCues->Length() == 0);
  if (c1 && c2 && c3) {
    mLastTimeMarchesOnCalled = currentPlaybackTime;
    return;
  }

  // Step 8. Respect PauseOnExit flag if not seek.
  if (hasNormalPlayback) {
    for (uint32_t i = 0; i < otherCues->Length(); ++i) {
      TextTrackCue* cue = (*otherCues)[i];
      if (cue && cue->PauseOnExit() && cue->GetActive()) {
        mMediaElement->Pause();
        break;
      }
    }
    for (uint32_t i = 0; i < missedCues->Length(); ++i) {
      TextTrackCue* cue = (*missedCues)[i];
      if (cue && cue->PauseOnExit()) {
        mMediaElement->Pause();
        break;
      }
    }
  }

  // Step 15.
  // Sort text tracks in the same order as the text tracks appear
  // in the media element's list of text tracks, and remove
  // duplicates.
  TextTrackListInternal affectedTracks;
  // Step 13, 14.
  nsTArray<RefPtr<SimpleTextTrackEvent>> eventList;
  // Step 9, 10.
  // For each text track cue in missed cues, prepare an event named
  // enter for the TextTrackCue object with the cue start time.
  for (uint32_t i = 0; i < missedCues->Length(); ++i) {
    TextTrackCue* cue = (*missedCues)[i];
    if (cue) {
      SimpleTextTrackEvent* event =
        new SimpleTextTrackEvent(NS_LITERAL_STRING("enter"),
                                 cue->StartTime(), cue->GetTrack(),
                                 cue);
      eventList.InsertElementSorted(event,
        CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(), CompareTextTracks(mMediaElement));
    }
  }

  // Step 11, 17.
  for (uint32_t i = 0; i < otherCues->Length(); ++i) {
    TextTrackCue* cue = (*otherCues)[i];
    if (cue->GetActive() ||
        missedCues->GetCueById(cue->Id()) != nullptr) {
      double time = cue->StartTime() > cue->EndTime() ? cue->StartTime()
                                                      : cue->EndTime();
      SimpleTextTrackEvent* event =
        new SimpleTextTrackEvent(NS_LITERAL_STRING("exit"), time,
                                 cue->GetTrack(), cue);
      eventList.InsertElementSorted(event,
        CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(), CompareTextTracks(mMediaElement));
    }
    cue->SetActive(false);
  }

  // Step 12, 17.
  for (uint32_t i = 0; i < currentCues->Length(); ++i) {
    TextTrackCue* cue = (*currentCues)[i];
    if (!cue->GetActive()) {
      SimpleTextTrackEvent* event =
        new SimpleTextTrackEvent(NS_LITERAL_STRING("enter"),
                                 cue->StartTime(), cue->GetTrack(),
                                 cue);
      eventList.InsertElementSorted(event,
        CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(), CompareTextTracks(mMediaElement));
    }
    cue->SetActive(true);
  }

  // Fire the eventList
  for (uint32_t i = 0; i < eventList.Length(); ++i) {
    NS_DispatchToMainThread(eventList[i].forget());
  }

  // Step 16.
  for (uint32_t i = 0; i < affectedTracks.Length(); ++i) {
    TextTrack* ttrack = affectedTracks[i];
    if (ttrack) {
      ttrack->DispatchTrustedEvent(NS_LITERAL_STRING("cuechange"));
      HTMLTrackElement* trackElement = ttrack->GetTrackElement();
      if (trackElement) {
        trackElement->DispatchTrackRunnable(NS_LITERAL_STRING("cuechange"));
      }
    }
  }

  mLastTimeMarchesOnCalled = currentPlaybackTime;
  mLastActiveCues = currentCues;

  // Step 18.
  UpdateCueDisplay();
}

} // namespace dom
} // namespace mozilla
