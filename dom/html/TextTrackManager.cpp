/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackCue.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindow.h"
#include "nsIFrame.h"
#include "nsIWebVTTParserWrapper.h"
#include "nsVariant.h"
#include "nsVideoFrame.h"

mozilla::LazyLogModule gTextTrackLog("WebVTT");

#define WEBVTT_LOG(msg, ...)              \
  MOZ_LOG(gTextTrackLog, LogLevel::Debug, \
          ("TextTrackManager=%p, " msg, this, ##__VA_ARGS__))
#define WEBVTT_LOGV(msg, ...)               \
  MOZ_LOG(gTextTrackLog, LogLevel::Verbose, \
          ("TextTrackManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TextTrackManager::ShutdownObserverProxy, nsIObserver);

void TextTrackManager::ShutdownObserverProxy::Unregister() {
  nsContentUtils::UnregisterShutdownObserver(this);
  mManager = nullptr;
}

CompareTextTracks::CompareTextTracks(HTMLMediaElement* aMediaElement) {
  mMediaElement = aMediaElement;
}

int32_t CompareTextTracks::TrackChildPosition(TextTrack* aTextTrack) const {
  MOZ_DIAGNOSTIC_ASSERT(aTextTrack);
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return -1;
  }
  return mMediaElement->ComputeIndexOf(trackElement);
}

bool CompareTextTracks::Equals(TextTrack* aOne, TextTrack* aTwo) const {
  // Two tracks can never be equal. If they have corresponding TrackElements
  // they would need to occupy the same tree position (impossible) and in the
  // case of tracks coming from AddTextTrack source we put the newest at the
  // last position, so they won't be equal as well.
  return false;
}

bool CompareTextTracks::LessThan(TextTrack* aOne, TextTrack* aTwo) const {
  // Protect against nullptr TextTrack objects; treat them as
  // sorting toward the end.
  if (!aOne) {
    return false;
  }
  if (!aTwo) {
    return true;
  }
  TextTrackSource sourceOne = aOne->GetTextTrackSource();
  TextTrackSource sourceTwo = aTwo->GetTextTrackSource();
  if (sourceOne != sourceTwo) {
    return sourceOne == TextTrackSource::Track ||
           (sourceOne == TextTrackSource::AddTextTrack &&
            sourceTwo == TextTrackSource::MediaResourceSpecific);
  }
  switch (sourceOne) {
    case TextTrackSource::Track: {
      int32_t positionOne = TrackChildPosition(aOne);
      int32_t positionTwo = TrackChildPosition(aTwo);
      // If either position one or positiontwo are -1 then something has gone
      // wrong. In this case we should just put them at the back of the list.
      return positionOne != -1 && positionTwo != -1 &&
             positionOne < positionTwo;
    }
    case TextTrackSource::AddTextTrack:
      // For AddTextTrack sources the tracks will already be in the correct
      // relative order in the source array. Assume we're called in iteration
      // order and can therefore always report aOne < aTwo to maintain the
      // original temporal ordering.
      return true;
    case TextTrackSource::MediaResourceSpecific:
      // No rules for Media Resource Specific tracks yet.
      break;
  }
  return true;
}

NS_IMPL_CYCLE_COLLECTION(TextTrackManager, mMediaElement, mTextTracks,
                         mPendingTextTracks, mNewCues)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackManager)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackManager::sParserWrapper;

TextTrackManager::TextTrackManager(HTMLMediaElement* aMediaElement)
    : mMediaElement(aMediaElement),
      mHasSeeked(false),
      mLastTimeMarchesOnCalled(media::TimeUnit::Zero()),
      mTimeMarchesOnDispatched(false),
      mUpdateCueDisplayDispatched(false),
      performedTrackSelection(false),
      mShutdown(false) {
  nsISupports* parentObject = mMediaElement->OwnerDoc()->GetParentObject();

  NS_ENSURE_TRUE_VOID(parentObject);
  WEBVTT_LOG("Create TextTrackManager");
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);
  mNewCues = new TextTrackCueList(window);
  mTextTracks = new TextTrackList(window, this);
  mPendingTextTracks = new TextTrackList(window, this);

  if (!sParserWrapper) {
    nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
        do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID);
    MOZ_ASSERT(parserWrapper, "Can't create nsIWebVTTParserWrapper");
    sParserWrapper = parserWrapper;
    ClearOnShutdown(&sParserWrapper);
  }
  mShutdownProxy = new ShutdownObserverProxy(this);
}

TextTrackManager::~TextTrackManager() {
  WEBVTT_LOG("~TextTrackManager");
  mShutdownProxy->Unregister();
}

TextTrackList* TextTrackManager::GetTextTracks() const { return mTextTracks; }

already_AddRefed<TextTrack> TextTrackManager::AddTextTrack(
    TextTrackKind aKind, const nsAString& aLabel, const nsAString& aLanguage,
    TextTrackMode aMode, TextTrackReadyState aReadyState,
    TextTrackSource aTextTrackSource) {
  if (!mMediaElement || !mTextTracks) {
    return nullptr;
  }
  RefPtr<TextTrack> track = mTextTracks->AddTextTrack(
      aKind, aLabel, aLanguage, aMode, aReadyState, aTextTrackSource,
      CompareTextTracks(mMediaElement));
  WEBVTT_LOG("AddTextTrack %p kind %" PRIu32 " Label %s Language %s",
             track.get(), static_cast<uint32_t>(aKind),
             NS_ConvertUTF16toUTF8(aLabel).get(),
             NS_ConvertUTF16toUTF8(aLanguage).get());
  AddCues(track);
  ReportTelemetryForTrack(track);

  if (aTextTrackSource == TextTrackSource::Track) {
    RefPtr<nsIRunnable> task = NewRunnableMethod(
        "dom::TextTrackManager::HonorUserPreferencesForTrackSelection", this,
        &TextTrackManager::HonorUserPreferencesForTrackSelection);
    NS_DispatchToMainThread(task.forget());
  }

  return track.forget();
}

void TextTrackManager::AddTextTrack(TextTrack* aTextTrack) {
  if (!mMediaElement || !mTextTracks) {
    return;
  }
  WEBVTT_LOG("AddTextTrack TextTrack %p", aTextTrack);
  mTextTracks->AddTextTrack(aTextTrack, CompareTextTracks(mMediaElement));
  AddCues(aTextTrack);
  ReportTelemetryForTrack(aTextTrack);

  if (aTextTrack->GetTextTrackSource() == TextTrackSource::Track) {
    RefPtr<nsIRunnable> task = NewRunnableMethod(
        "dom::TextTrackManager::HonorUserPreferencesForTrackSelection", this,
        &TextTrackManager::HonorUserPreferencesForTrackSelection);
    NS_DispatchToMainThread(task.forget());
  }
}

void TextTrackManager::AddCues(TextTrack* aTextTrack) {
  if (!mNewCues) {
    WEBVTT_LOG("AddCues mNewCues is null");
    return;
  }

  TextTrackCueList* cueList = aTextTrack->GetCues();
  if (cueList) {
    bool dummy;
    WEBVTT_LOGV("AddCues, CuesNum=%d", cueList->Length());
    for (uint32_t i = 0; i < cueList->Length(); ++i) {
      mNewCues->AddCue(*cueList->IndexedGetter(i, dummy));
    }
    MaybeRunTimeMarchesOn();
  }
}

void TextTrackManager::RemoveTextTrack(TextTrack* aTextTrack,
                                       bool aPendingListOnly) {
  if (!mPendingTextTracks || !mTextTracks) {
    return;
  }

  WEBVTT_LOG("RemoveTextTrack TextTrack %p", aTextTrack);
  mPendingTextTracks->RemoveTextTrack(aTextTrack);
  if (aPendingListOnly) {
    return;
  }

  mTextTracks->RemoveTextTrack(aTextTrack);
  // Remove the cues in mNewCues belong to aTextTrack.
  TextTrackCueList* removeCueList = aTextTrack->GetCues();
  if (removeCueList) {
    WEBVTT_LOGV("RemoveTextTrack removeCuesNum=%d", removeCueList->Length());
    for (uint32_t i = 0; i < removeCueList->Length(); ++i) {
      mNewCues->RemoveCue(*((*removeCueList)[i]));
    }
    MaybeRunTimeMarchesOn();
  }
}

void TextTrackManager::DidSeek() {
  WEBVTT_LOG("DidSeek");
  mHasSeeked = true;
}

void TextTrackManager::UpdateCueDisplay() {
  WEBVTT_LOG("UpdateCueDisplay");
  mUpdateCueDisplayDispatched = false;

  if (!mMediaElement || !mTextTracks || IsShutdown()) {
    WEBVTT_LOG("Abort UpdateCueDisplay.");
    return;
  }

  nsIFrame* frame = mMediaElement->GetPrimaryFrame();
  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    WEBVTT_LOG("Abort UpdateCueDisplay, because of no video frame.");
    return;
  }

  nsCOMPtr<nsIContent> overlay = videoFrame->GetCaptionOverlay();
  if (!overlay) {
    WEBVTT_LOG("Abort UpdateCueDisplay, because of no overlay.");
    return;
  }

  nsPIDOMWindowInner* window = mMediaElement->OwnerDoc()->GetInnerWindow();
  if (!window) {
    WEBVTT_LOG("Abort UpdateCueDisplay, because of no window.");
  }

  nsTArray<RefPtr<TextTrackCue>> showingCues;
  mTextTracks->GetShowingCues(showingCues);

  WEBVTT_LOG("UpdateCueDisplay, processCues, showingCuesNum=%zu",
             showingCues.Length());
  RefPtr<nsVariantCC> jsCues = new nsVariantCC();
  jsCues->SetAsArray(nsIDataType::VTYPE_INTERFACE, &NS_GET_IID(EventTarget),
                     showingCues.Length(),
                     static_cast<void*>(showingCues.Elements()));
  nsCOMPtr<nsIContent> controls = videoFrame->GetVideoControls();
  sParserWrapper->ProcessCues(window, jsCues, overlay, controls);
}

void TextTrackManager::NotifyCueAdded(TextTrackCue& aCue) {
  WEBVTT_LOG("NotifyCueAdded, cue=%p", &aCue);
  if (mNewCues) {
    mNewCues->AddCue(aCue);
  }
  MaybeRunTimeMarchesOn();
}

void TextTrackManager::NotifyCueRemoved(TextTrackCue& aCue) {
  WEBVTT_LOG("NotifyCueRemoved, cue=%p", &aCue);
  if (mNewCues) {
    mNewCues->RemoveCue(aCue);
  }
  MaybeRunTimeMarchesOn();
  DispatchUpdateCueDisplay();
}

void TextTrackManager::PopulatePendingList() {
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

void TextTrackManager::AddListeners() {
  if (mMediaElement) {
    mMediaElement->AddEventListener(NS_LITERAL_STRING("resizecaption"), this,
                                    false, false);
    mMediaElement->AddEventListener(NS_LITERAL_STRING("resizevideocontrols"),
                                    this, false, false);
    mMediaElement->AddEventListener(NS_LITERAL_STRING("seeked"), this, false,
                                    false);
    mMediaElement->AddEventListener(NS_LITERAL_STRING("controlbarchange"), this,
                                    false, true);
  }
}

void TextTrackManager::HonorUserPreferencesForTrackSelection() {
  if (performedTrackSelection || !mTextTracks) {
    return;
  }
  WEBVTT_LOG("HonorUserPreferencesForTrackSelection");
  TextTrackKind ttKinds[] = {TextTrackKind::Captions, TextTrackKind::Subtitles};

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

bool TextTrackManager::TrackIsDefault(TextTrack* aTextTrack) {
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return false;
  }
  return trackElement->Default();
}

void TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKind) {
  TextTrackKind ttKinds[] = {aTextTrackKind};
  PerformTrackSelection(ttKinds, ArrayLength(ttKinds));
}

void TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKinds[],
                                             uint32_t size) {
  nsTArray<TextTrack*> candidates;
  GetTextTracksOfKinds(aTextTrackKinds, size, candidates);

  // Step 3: If any TextTracks in candidates are showing then abort these steps.
  for (uint32_t i = 0; i < candidates.Length(); i++) {
    if (candidates[i]->Mode() == TextTrackMode::Showing) {
      WEBVTT_LOGV("PerformTrackSelection Showing return kind %d",
                  static_cast<int>(candidates[i]->Kind()));
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
      WEBVTT_LOGV("PerformTrackSelection set Showing kind %d",
                  static_cast<int>(candidates[i]->Kind()));
      return;
    }
  }
}

void TextTrackManager::GetTextTracksOfKinds(TextTrackKind aTextTrackKinds[],
                                            uint32_t size,
                                            nsTArray<TextTrack*>& aTextTracks) {
  for (uint32_t i = 0; i < size; i++) {
    GetTextTracksOfKind(aTextTrackKinds[i], aTextTracks);
  }
}

void TextTrackManager::GetTextTracksOfKind(TextTrackKind aTextTrackKind,
                                           nsTArray<TextTrack*>& aTextTracks) {
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
TextTrackManager::HandleEvent(Event* aEvent) {
  if (!mTextTracks) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);
  WEBVTT_LOG("Handle event %s", NS_ConvertUTF16toUTF8(type).get());

  const bool setDirty = type.EqualsLiteral("seeked") ||
                        type.EqualsLiteral("resizecaption") ||
                        type.EqualsLiteral("resizevideocontrols");
  const bool updateDisplay = type.EqualsLiteral("controlbarchange") ||
                             type.EqualsLiteral("resizecaption");

  if (setDirty) {
    for (uint32_t i = 0; i < mTextTracks->Length(); i++) {
      ((*mTextTracks)[i])->SetCuesDirty();
    }
  }
  if (updateDisplay) {
    UpdateCueDisplay();
  }

  return NS_OK;
}

class SimpleTextTrackEvent : public Runnable {
 public:
  friend class CompareSimpleTextTrackEvents;
  SimpleTextTrackEvent(const nsAString& aEventName, double aTime,
                       TextTrack* aTrack, TextTrackCue* aCue)
      : Runnable("dom::SimpleTextTrackEvent"),
        mName(aEventName),
        mTime(aTime),
        mTrack(aTrack),
        mCue(aCue) {}

  NS_IMETHOD Run() override {
    WEBVTT_LOGV("SimpleTextTrackEvent cue %p mName %s mTime %lf", mCue.get(),
                NS_ConvertUTF16toUTF8(mName).get(), mTime);
    mCue->DispatchTrustedEvent(mName);
    return NS_OK;
  }

  void Dispatch() {
    if (nsCOMPtr<nsIGlobalObject> global = mCue->GetOwnerGlobal()) {
      global->Dispatch(TaskCategory::Other, do_AddRef(this));
    } else {
      NS_DispatchToMainThread(do_AddRef(this));
    }
  }

 private:
  nsString mName;
  double mTime;
  TextTrack* mTrack;
  RefPtr<TextTrackCue> mCue;
};

class CompareSimpleTextTrackEvents {
 private:
  int32_t TrackChildPosition(SimpleTextTrackEvent* aEvent) const {
    if (aEvent->mTrack) {
      HTMLTrackElement* trackElement = aEvent->mTrack->GetTrackElement();
      if (trackElement) {
        return mMediaElement->ComputeIndexOf(trackElement);
      }
    }
    return -1;
  }
  HTMLMediaElement* mMediaElement;

 public:
  explicit CompareSimpleTextTrackEvents(HTMLMediaElement* aMediaElement) {
    mMediaElement = aMediaElement;
  }

  bool Equals(SimpleTextTrackEvent* aOne, SimpleTextTrackEvent* aTwo) const {
    return false;
  }

  bool LessThan(SimpleTextTrackEvent* aOne, SimpleTextTrackEvent* aTwo) const {
    // TimeMarchesOn step 13.1.
    if (aOne->mTime < aTwo->mTime) {
      return true;
    }
    if (aOne->mTime > aTwo->mTime) {
      return false;
    }

    // TimeMarchesOn step 13.2 text track cue order.
    // TextTrack position in TextTrackList
    TextTrack* t1 = aOne->mTrack;
    TextTrack* t2 = aTwo->mTrack;
    MOZ_ASSERT(t1, "CompareSimpleTextTrackEvents t1 is null");
    MOZ_ASSERT(t2, "CompareSimpleTextTrackEvents t2 is null");
    if (t1 != t2) {
      TextTrackList* tList = t1->GetTextTrackList();
      MOZ_ASSERT(tList, "CompareSimpleTextTrackEvents tList is null");
      nsTArray<RefPtr<TextTrack>>& textTracks = tList->GetTextTrackArray();
      auto index1 = textTracks.IndexOf(t1);
      auto index2 = textTracks.IndexOf(t2);
      if (index1 < index2) {
        return true;
      }
      if (index1 > index2) {
        return false;
      }
    }

    MOZ_ASSERT(t1 == t2, "CompareSimpleTextTrackEvents t1 != t2");
    // c1 and c2 are both belongs to t1.
    TextTrackCue* c1 = aOne->mCue;
    TextTrackCue* c2 = aTwo->mCue;
    if (c1 != c2) {
      if (c1->StartTime() < c2->StartTime()) {
        return true;
      }
      if (c1->StartTime() > c2->StartTime()) {
        return false;
      }
      if (c1->EndTime() < c2->EndTime()) {
        return true;
      }
      if (c1->EndTime() > c2->EndTime()) {
        return false;
      }

      TextTrackCueList* cueList = t1->GetCues();
      MOZ_ASSERT(cueList);
      nsTArray<RefPtr<TextTrackCue>>& cues = cueList->GetCuesArray();
      auto index1 = cues.IndexOf(c1);
      auto index2 = cues.IndexOf(c2);
      if (index1 < index2) {
        return true;
      }
      if (index1 > index2) {
        return false;
      }
    }

    // TimeMarchesOn step 13.3.
    if (aOne->mName.EqualsLiteral("enter") ||
        aTwo->mName.EqualsLiteral("exit")) {
      return true;
    }
    return false;
  }
};

class TextTrackListInternal {
 public:
  void AddTextTrack(TextTrack* aTextTrack,
                    const CompareTextTracks& aCompareTT) {
    if (!mTextTracks.Contains(aTextTrack)) {
      mTextTracks.InsertElementSorted(aTextTrack, aCompareTT);
    }
  }
  uint32_t Length() const { return mTextTracks.Length(); }
  TextTrack* operator[](uint32_t aIndex) {
    return mTextTracks.SafeElementAt(aIndex, nullptr);
  }

 private:
  nsTArray<RefPtr<TextTrack>> mTextTracks;
};

void TextTrackManager::DispatchUpdateCueDisplay() {
  if (!mUpdateCueDisplayDispatched && !IsShutdown()) {
    WEBVTT_LOG("DispatchUpdateCueDisplay");
    nsPIDOMWindowInner* win = mMediaElement->OwnerDoc()->GetInnerWindow();
    if (win) {
      nsGlobalWindowInner::Cast(win)->Dispatch(
          TaskCategory::Other,
          NewRunnableMethod("dom::TextTrackManager::UpdateCueDisplay", this,
                            &TextTrackManager::UpdateCueDisplay));
      mUpdateCueDisplayDispatched = true;
    }
  }
}

void TextTrackManager::DispatchTimeMarchesOn() {
  // Run the algorithm if no previous instance is still running, otherwise
  // enqueue the current playback position and whether only that changed
  // through its usual monotonic increase during normal playback; current
  // executing call upon completion will check queue for further 'work'.
  if (!mTimeMarchesOnDispatched && !IsShutdown()) {
    WEBVTT_LOG("DispatchTimeMarchesOn");
    nsPIDOMWindowInner* win = mMediaElement->OwnerDoc()->GetInnerWindow();
    if (win) {
      nsGlobalWindowInner::Cast(win)->Dispatch(
          TaskCategory::Other,
          NewRunnableMethod("dom::TextTrackManager::TimeMarchesOn", this,
                            &TextTrackManager::TimeMarchesOn));
      mTimeMarchesOnDispatched = true;
    }
  }
}

// https://html.spec.whatwg.org/multipage/embedded-content.html#time-marches-on
void TextTrackManager::TimeMarchesOn() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mTimeMarchesOnDispatched = false;

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();
  if (context && context->IsInStableOrMetaStableState()) {
    // FireTimeUpdate can be called while at stable state following a
    // current position change which triggered a state watcher in MediaDecoder
    // (see bug 1443429).
    // TimeMarchesOn() will modify JS attributes which is forbidden while in
    // stable state. So we dispatch a task to perform such operation later
    // instead.
    DispatchTimeMarchesOn();
    return;
  }
  WEBVTT_LOG("TimeMarchesOn");

  // Early return if we don't have any TextTracks or shutting down.
  if (!mTextTracks || mTextTracks->Length() == 0 || IsShutdown() ||
      !mMediaElement) {
    return;
  }

  if (mMediaElement->ReadyState() == HTMLMediaElement_Binding::HAVE_NOTHING) {
    WEBVTT_LOG(
        "TimeMarchesOn return because media doesn't contain any data yet");
    return;
  }

  if (mMediaElement->Seeking()) {
    WEBVTT_LOG("TimeMarchesOn return during seeking");
    return;
  }

  // Step 1, 2.
  nsISupports* parentObject = mMediaElement->OwnerDoc()->GetParentObject();
  if (NS_WARN_IF(!parentObject)) {
    return;
  }
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);
  RefPtr<TextTrackCueList> currentCues = new TextTrackCueList(window);
  RefPtr<TextTrackCueList> otherCues = new TextTrackCueList(window);

  // Step 3.
  auto currentPlaybackTime =
      media::TimeUnit::FromSeconds(mMediaElement->CurrentTime());
  bool hasNormalPlayback = !mHasSeeked;
  mHasSeeked = false;
  WEBVTT_LOG(
      "TimeMarchesOn mLastTimeMarchesOnCalled %lf currentPlaybackTime %lf "
      "hasNormalPlayback %d",
      mLastTimeMarchesOnCalled.ToSeconds(), currentPlaybackTime.ToSeconds(),
      hasNormalPlayback);

  // The reason we collect other cues is (1) to change active cues to inactive,
  // (2) find missing cues, so we actually no need to process all cues. We just
  // need to handle cues which are in the time interval [lastTime:currentTime]
  // or [currentTime:lastTime] (seeking forward). That can help us to reduce the
  // size of other cues, which can improve execution time.
  auto start = std::min(mLastTimeMarchesOnCalled, currentPlaybackTime);
  auto end = std::max(mLastTimeMarchesOnCalled, currentPlaybackTime);
  media::TimeInterval interval(start, end);
  WEBVTT_LOGV("TimeMarchesOn Time interval [%f:%f]", start.ToSeconds(),
              end.ToSeconds());
  for (uint32_t idx = 0; idx < mTextTracks->Length(); ++idx) {
    TextTrack* track = (*mTextTracks)[idx];
    if (track) {
      track->GetCurrentCuesAndOtherCues(currentCues, otherCues, interval);
    }
  }

  // Step 4.
  RefPtr<TextTrackCueList> missedCues = new TextTrackCueList(window);
  if (hasNormalPlayback) {
    for (uint32_t i = 0; i < otherCues->Length(); ++i) {
      TextTrackCue* cue = (*otherCues)[i];
      if (cue->StartTime() >= mLastTimeMarchesOnCalled.ToSeconds() &&
          cue->EndTime() <= currentPlaybackTime.ToSeconds()) {
        missedCues->AddCue(*cue);
      }
    }
  }

  WEBVTT_LOGV("TimeMarchesOn currentCues %d", currentCues->Length());
  WEBVTT_LOGV("TimeMarchesOn otherCues %d", otherCues->Length());
  WEBVTT_LOGV("TimeMarchesOn missedCues %d", missedCues->Length());
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
    WEBVTT_LOG("TimeMarchesOn step 7 return, mLastTimeMarchesOnCalled %lf",
               mLastTimeMarchesOnCalled.ToSeconds());
    return;
  }

  // Step 8. Respect PauseOnExit flag if not seek.
  if (hasNormalPlayback) {
    for (uint32_t i = 0; i < otherCues->Length(); ++i) {
      TextTrackCue* cue = (*otherCues)[i];
      if (cue && cue->PauseOnExit() && cue->GetActive()) {
        WEBVTT_LOG("TimeMarchesOn pause the MediaElement");
        mMediaElement->Pause();
        break;
      }
    }
    for (uint32_t i = 0; i < missedCues->Length(); ++i) {
      TextTrackCue* cue = (*missedCues)[i];
      if (cue && cue->PauseOnExit()) {
        WEBVTT_LOG("TimeMarchesOn pause the MediaElement");
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
      WEBVTT_LOG("Prepare 'enter' event for cue %p [%f, %f] in missing cues",
                 cue, cue->StartTime(), cue->EndTime());
      SimpleTextTrackEvent* event = new SimpleTextTrackEvent(
          NS_LITERAL_STRING("enter"), cue->StartTime(), cue->GetTrack(), cue);
      eventList.InsertElementSorted(
          event, CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(),
                                  CompareTextTracks(mMediaElement));
    }
  }

  // Step 11, 17.
  for (uint32_t i = 0; i < otherCues->Length(); ++i) {
    TextTrackCue* cue = (*otherCues)[i];
    if (cue->GetActive() || missedCues->IsCueExist(cue)) {
      double time =
          cue->StartTime() > cue->EndTime() ? cue->StartTime() : cue->EndTime();
      WEBVTT_LOG("Prepare 'exit' event for cue %p [%f, %f] in other cues", cue,
                 cue->StartTime(), cue->EndTime());
      SimpleTextTrackEvent* event = new SimpleTextTrackEvent(
          NS_LITERAL_STRING("exit"), time, cue->GetTrack(), cue);
      eventList.InsertElementSorted(
          event, CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(),
                                  CompareTextTracks(mMediaElement));
    }
    cue->SetActive(false);
  }

  // Step 12, 17.
  for (uint32_t i = 0; i < currentCues->Length(); ++i) {
    TextTrackCue* cue = (*currentCues)[i];
    if (!cue->GetActive()) {
      WEBVTT_LOG("Prepare 'enter' event for cue %p [%f, %f] in current cues",
                 cue, cue->StartTime(), cue->EndTime());
      SimpleTextTrackEvent* event = new SimpleTextTrackEvent(
          NS_LITERAL_STRING("enter"), cue->StartTime(), cue->GetTrack(), cue);
      eventList.InsertElementSorted(
          event, CompareSimpleTextTrackEvents(mMediaElement));
      affectedTracks.AddTextTrack(cue->GetTrack(),
                                  CompareTextTracks(mMediaElement));
    }
    cue->SetActive(true);
  }

  // Fire the eventList
  for (uint32_t i = 0; i < eventList.Length(); ++i) {
    eventList[i]->Dispatch();
  }

  // Step 16.
  for (uint32_t i = 0; i < affectedTracks.Length(); ++i) {
    TextTrack* ttrack = affectedTracks[i];
    if (ttrack) {
      ttrack->DispatchAsyncTrustedEvent(NS_LITERAL_STRING("cuechange"));
      HTMLTrackElement* trackElement = ttrack->GetTrackElement();
      if (trackElement) {
        trackElement->DispatchTrackRunnable(NS_LITERAL_STRING("cuechange"));
      }
    }
  }

  mLastTimeMarchesOnCalled = currentPlaybackTime;

  // Step 18.
  UpdateCueDisplay();
}

void TextTrackManager::NotifyCueUpdated(TextTrackCue* aCue) {
  // TODO: Add/Reorder the cue to mNewCues if we have some optimization?
  WEBVTT_LOG("NotifyCueUpdated, cue=%p", aCue);
  MaybeRunTimeMarchesOn();
  // For the case "Texttrack.mode = hidden/showing", if the mode
  // changing between showing and hidden, TimeMarchesOn
  // doesn't render the cue. Call DispatchUpdateCueDisplay() explicitly.
  DispatchUpdateCueDisplay();
}

void TextTrackManager::NotifyReset() {
  // https://html.spec.whatwg.org/multipage/media.html#text-track-cue-active-flag
  // This will unset all cues' active flag and update the cue display.
  WEBVTT_LOG("NotifyReset");
  mLastTimeMarchesOnCalled = media::TimeUnit::Zero();
  for (uint32_t idx = 0; idx < mTextTracks->Length(); ++idx) {
    (*mTextTracks)[idx]->SetCuesInactive();
  }
  UpdateCueDisplay();
}

void TextTrackManager::ReportTelemetryForTrack(TextTrack* aTextTrack) const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTextTrack);
  MOZ_ASSERT(mTextTracks->Length() > 0);

  TextTrackKind kind = aTextTrack->Kind();
  Telemetry::Accumulate(Telemetry::WEBVTT_TRACK_KINDS, uint32_t(kind));
}

bool TextTrackManager::IsLoaded() {
  return mTextTracks ? mTextTracks->AreTextTracksLoaded() : true;
}

bool TextTrackManager::IsShutdown() const {
  return (mShutdown || !sParserWrapper);
}

void TextTrackManager::MaybeRunTimeMarchesOn() {
  MOZ_ASSERT(mMediaElement);
  // According to spec, we should check media element's show poster flag before
  // running `TimeMarchesOn` in following situations, (1) add cue (2) remove cue
  // (3) cue's start time changes (4) cues's end time changes
  // https://html.spec.whatwg.org/multipage/media.html#playing-the-media-resource:time-marches-on
  // https://html.spec.whatwg.org/multipage/media.html#text-track-api:time-marches-on
  if (mMediaElement->GetShowPosterFlag()) {
    return;
  }
  TimeMarchesOn();
}

}  // namespace dom
}  // namespace mozilla
