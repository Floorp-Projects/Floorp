/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "nsGlobalWindow.h"

extern mozilla::LazyLogModule gTextTrackLog;

#define WEBVTT_LOG(msg, ...)              \
  MOZ_LOG(gTextTrackLog, LogLevel::Debug, \
          ("TextTrack=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static const char* ToStateStr(const TextTrackMode aMode) {
  switch (aMode) {
    case TextTrackMode::Disabled:
      return "DISABLED";
    case TextTrackMode::Hidden:
      return "HIDDEN";
    case TextTrackMode::Showing:
      return "SHOWING";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid state.");
  }
  return "Unknown";
}

static const char* ToReadyStateStr(const TextTrackReadyState aState) {
  switch (aState) {
    case TextTrackReadyState::NotLoaded:
      return "NotLoaded";
    case TextTrackReadyState::Loading:
      return "Loading";
    case TextTrackReadyState::Loaded:
      return "Loaded";
    case TextTrackReadyState::FailedToLoad:
      return "FailedToLoad";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid state.");
  }
  return "Unknown";
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TextTrack, DOMEventTargetHelper, mCueList,
                                   mActiveCueList, mTextTrackList,
                                   mTrackElement)

NS_IMPL_ADDREF_INHERITED(TextTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TextTrack::TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackKind aKind,
                     const nsAString& aLabel, const nsAString& aLanguage,
                     TextTrackMode aMode, TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
    : DOMEventTargetHelper(aOwnerWindow),
      mKind(aKind),
      mLabel(aLabel),
      mLanguage(aLanguage),
      mMode(aMode),
      mReadyState(aReadyState),
      mTextTrackSource(aTextTrackSource) {
  SetDefaultSettings();
}

TextTrack::TextTrack(nsPIDOMWindowInner* aOwnerWindow,
                     TextTrackList* aTextTrackList, TextTrackKind aKind,
                     const nsAString& aLabel, const nsAString& aLanguage,
                     TextTrackMode aMode, TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
    : DOMEventTargetHelper(aOwnerWindow),
      mTextTrackList(aTextTrackList),
      mKind(aKind),
      mLabel(aLabel),
      mLanguage(aLanguage),
      mMode(aMode),
      mReadyState(aReadyState),
      mTextTrackSource(aTextTrackSource) {
  SetDefaultSettings();
}

TextTrack::~TextTrack() {}

void TextTrack::SetDefaultSettings() {
  nsPIDOMWindowInner* ownerWindow = GetOwner();
  mCueList = new TextTrackCueList(ownerWindow);
  mActiveCueList = new TextTrackCueList(ownerWindow);
  mCuePos = 0;
  mDirty = false;
}

JSObject* TextTrack::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return TextTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void TextTrack::SetMode(TextTrackMode aValue) {
  if (mMode == aValue) {
    return;
  }
  WEBVTT_LOG("Set mode=%s", ToStateStr(aValue));
  mMode = aValue;

  HTMLMediaElement* mediaElement = GetMediaElement();
  if (aValue == TextTrackMode::Disabled) {
    for (size_t i = 0; i < mCueList->Length() && mediaElement; ++i) {
      mediaElement->NotifyCueRemoved(*(*mCueList)[i]);
    }
    SetCuesInactive();
  } else {
    for (size_t i = 0; i < mCueList->Length() && mediaElement; ++i) {
      mediaElement->NotifyCueAdded(*(*mCueList)[i]);
    }
  }
  if (mediaElement) {
    mediaElement->NotifyTextTrackModeChanged();
  }
  // Ensure the TimeMarchesOn is called in case that the mCueList
  // is empty.
  NotifyCueUpdated(nullptr);
}

void TextTrack::GetId(nsAString& aId) const {
  // If the track has a track element then its id should be the same as the
  // track element's id.
  if (mTrackElement) {
    mTrackElement->GetAttribute(NS_LITERAL_STRING("id"), aId);
  }
}

void TextTrack::AddCue(TextTrackCue& aCue) {
  WEBVTT_LOG("AddCue %p", &aCue);
  TextTrack* oldTextTrack = aCue.GetTrack();
  if (oldTextTrack) {
    ErrorResult dummy;
    oldTextTrack->RemoveCue(aCue, dummy);
  }
  mCueList->AddCue(aCue);
  aCue.SetTrack(this);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement && (mMode != TextTrackMode::Disabled)) {
    mediaElement->NotifyCueAdded(aCue);
  }
}

void TextTrack::RemoveCue(TextTrackCue& aCue, ErrorResult& aRv) {
  WEBVTT_LOG("RemoveCue %p", &aCue);
  // Bug1304948, check the aCue belongs to the TextTrack.
  mCueList->RemoveCue(aCue, aRv);
  if (aRv.Failed()) {
    return;
  }
  aCue.SetActive(false);
  aCue.SetTrack(nullptr);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement) {
    mediaElement->NotifyCueRemoved(aCue);
  }
}

void TextTrack::SetCuesDirty() {
  for (uint32_t i = 0; i < mCueList->Length(); i++) {
    ((*mCueList)[i])->Reset();
  }
}

TextTrackCueList* TextTrack::GetActiveCues() {
  if (mMode != TextTrackMode::Disabled) {
    return mActiveCueList;
  }
  return nullptr;
}

void TextTrack::GetActiveCueArray(nsTArray<RefPtr<TextTrackCue> >& aCues) {
  if (mMode != TextTrackMode::Disabled) {
    mActiveCueList->GetArray(aCues);
  }
}

TextTrackReadyState TextTrack::ReadyState() const { return mReadyState; }

void TextTrack::SetReadyState(uint32_t aReadyState) {
  if (aReadyState <= TextTrackReadyState::FailedToLoad) {
    SetReadyState(static_cast<TextTrackReadyState>(aReadyState));
  }
}

void TextTrack::SetReadyState(TextTrackReadyState aState) {
  WEBVTT_LOG("SetReadyState=%s", ToReadyStateStr(aState));
  mReadyState = aState;
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement && (mReadyState == TextTrackReadyState::Loaded ||
                       mReadyState == TextTrackReadyState::FailedToLoad)) {
    mediaElement->RemoveTextTrack(this, true);
    mediaElement->UpdateReadyState();
  }
}

TextTrackList* TextTrack::GetTextTrackList() { return mTextTrackList; }

void TextTrack::SetTextTrackList(TextTrackList* aTextTrackList) {
  mTextTrackList = aTextTrackList;
}

HTMLTrackElement* TextTrack::GetTrackElement() { return mTrackElement; }

void TextTrack::SetTrackElement(HTMLTrackElement* aTrackElement) {
  mTrackElement = aTrackElement;
}

void TextTrack::SetCuesInactive() {
  WEBVTT_LOG("SetCuesInactive");
  mCueList->SetCuesInactive();
}

void TextTrack::NotifyCueUpdated(TextTrackCue* aCue) {
  WEBVTT_LOG("NotifyCueUpdated, cue=%p", aCue);
  mCueList->NotifyCueUpdated(aCue);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement) {
    mediaElement->NotifyCueUpdated(aCue);
  }
}

void TextTrack::GetLabel(nsAString& aLabel) const {
  if (mTrackElement) {
    mTrackElement->GetLabel(aLabel);
  } else {
    aLabel = mLabel;
  }
}
void TextTrack::GetLanguage(nsAString& aLanguage) const {
  if (mTrackElement) {
    mTrackElement->GetSrclang(aLanguage);
  } else {
    aLanguage = mLanguage;
  }
}

void TextTrack::DispatchAsyncTrustedEvent(const nsString& aEventName) {
  nsPIDOMWindowInner* win = GetOwner();
  if (!win) {
    return;
  }
  RefPtr<TextTrack> self = this;
  nsGlobalWindowInner::Cast(win)->Dispatch(
      TaskCategory::Other,
      NS_NewRunnableFunction(
          "dom::TextTrack::DispatchAsyncTrustedEvent",
          [self, aEventName]() { self->DispatchTrustedEvent(aEventName); }));
}

bool TextTrack::IsLoaded() {
  if (mMode == TextTrackMode::Disabled) {
    return true;
  }
  // If the TrackElement's src is null, we can not block the
  // MediaElement.
  if (mTrackElement) {
    nsAutoString src;
    if (!(mTrackElement->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src))) {
      return true;
    }
  }
  return (mReadyState >= Loaded);
}

void TextTrack::NotifyCueActiveStateChanged(TextTrackCue* aCue) {
  MOZ_ASSERT(aCue);
  if (aCue->GetActive()) {
    MOZ_ASSERT(!mActiveCueList->IsCueExist(aCue));
    WEBVTT_LOG("NotifyCueActiveStateChanged, add cue %p to the active list",
               aCue);
    mActiveCueList->AddCue(*aCue);
  } else {
    MOZ_ASSERT(mActiveCueList->IsCueExist(aCue));
    WEBVTT_LOG(
        "NotifyCueActiveStateChanged, remove cue %p from the active list",
        aCue);
    mActiveCueList->RemoveCue(*aCue);
  }
}

void TextTrack::GetCurrentCuesAndOtherCues(
    RefPtr<TextTrackCueList>& aCurrentCues,
    RefPtr<TextTrackCueList>& aOtherCues,
    const media::TimeInterval& aInterval) const {
  const HTMLMediaElement* mediaElement = GetMediaElement();
  if (!mediaElement) {
    return;
  }

  if (Mode() == TextTrackMode::Disabled) {
    return;
  }

  // According to `time marches on` step1, current cue list contains the cues
  // whose start times are less than or equal to the current playback position
  // and whose end times are greater than the current playback position.
  // https://html.spec.whatwg.org/multipage/media.html#time-marches-on
  MOZ_ASSERT(aCurrentCues && aOtherCues);
  const double playbackTime = mediaElement->CurrentTime();
  for (uint32_t idx = 0; idx < mCueList->Length(); idx++) {
    TextTrackCue* cue = (*mCueList)[idx];
    if (cue->StartTime() <= playbackTime && cue->EndTime() > playbackTime) {
      WEBVTT_LOG("Add cue %p [%f:%f] to current cue list", cue,
                 cue->StartTime(), cue->EndTime());
      aCurrentCues->AddCue(*cue);
    } else {
      // As the spec didn't have a restriction for the negative duration, it
      // does happen sometime if user sets it explictly. It would be treated as
      // a `missing cue` later in the `TimeMarchesOn` but it won't be displayed.
      if (cue->EndTime() < cue->StartTime()) {
        // Add cue into `otherCue` only when its start time is contained by the
        // current time interval.
        if (aInterval.Contains(
                media::TimeUnit::FromSeconds(cue->StartTime()))) {
          WEBVTT_LOG("[Negative duration] Add cue %p [%f:%f] to other cue list",
                     cue, cue->StartTime(), cue->EndTime());
          aOtherCues->AddCue(*cue);
        }
        continue;
      }
      media::TimeInterval cueInterval(
          media::TimeUnit::FromSeconds(cue->StartTime()),
          media::TimeUnit::FromSeconds(cue->EndTime()));
      // cues are completely outside the time interval.
      if (!aInterval.Touches(cueInterval)) {
        continue;
      }
      // contains any cues which are overlapping within the time interval.
      WEBVTT_LOG("Add cue %p [%f:%f] to other cue list", cue, cue->StartTime(),
                 cue->EndTime());
      aOtherCues->AddCue(*cue);
    }
  }
}

HTMLMediaElement* TextTrack::GetMediaElement() const {
  return mTextTrackList ? mTextTrackList->GetMediaElement() : nullptr;
}

}  // namespace dom
}  // namespace mozilla
