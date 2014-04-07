/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_4(TextTrack,
                                     DOMEventTargetHelper,
                                     mCueList,
                                     mActiveCueList,
                                     mTextTrackList,
                                     mTrackElement)

NS_IMPL_ADDREF_INHERITED(TextTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TextTrack::TextTrack(nsPIDOMWindow* aOwnerWindow,
                     TextTrackKind aKind,
                     const nsAString& aLabel,
                     const nsAString& aLanguage,
                     TextTrackMode aMode,
                     TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
  : DOMEventTargetHelper(aOwnerWindow)
  , mKind(aKind)
  , mLabel(aLabel)
  , mLanguage(aLanguage)
  , mMode(aMode)
  , mReadyState(aReadyState)
  , mTextTrackSource(aTextTrackSource)
{
  SetDefaultSettings();
}

TextTrack::TextTrack(nsPIDOMWindow* aOwnerWindow,
                     TextTrackList* aTextTrackList,
                     TextTrackKind aKind,
                     const nsAString& aLabel,
                     const nsAString& aLanguage,
                     TextTrackMode aMode,
                     TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
  : DOMEventTargetHelper(aOwnerWindow)
  , mTextTrackList(aTextTrackList)
  , mKind(aKind)
  , mLabel(aLabel)
  , mLanguage(aLanguage)
  , mMode(aMode)
  , mReadyState(aReadyState)
  , mTextTrackSource(aTextTrackSource)
{
  SetDefaultSettings();
}

void
TextTrack::SetDefaultSettings()
{
  nsPIDOMWindow* ownerWindow = GetOwner();
  mCueList = new TextTrackCueList(ownerWindow);
  mActiveCueList = new TextTrackCueList(ownerWindow);
  mCuePos = 0;
  mDirty = false;
}

JSObject*
TextTrack::WrapObject(JSContext* aCx)
{
  return TextTrackBinding::Wrap(aCx, this);
}

void
TextTrack::SetMode(TextTrackMode aValue)
{
  if (mMode != aValue) {
    mMode = aValue;
    if (mTextTrackList) {
      mTextTrackList->CreateAndDispatchChangeEvent();
    }
  }
}

void
TextTrack::GetId(nsAString& aId) const
{
  // If the track has a track element then its id should be the same as the
  // track element's id.
  if (mTrackElement) {
    mTrackElement->GetAttribute(NS_LITERAL_STRING("id"), aId);
  }
}

void
TextTrack::AddCue(TextTrackCue& aCue)
{
  mCueList->AddCue(aCue);
  aCue.SetTrack(this);
  if (mTextTrackList) {
    HTMLMediaElement* mediaElement = mTextTrackList->GetMediaElement();
    if (mediaElement) {
      mediaElement->AddCue(aCue);
    }
  }
  SetDirty();
}

void
TextTrack::RemoveCue(TextTrackCue& aCue, ErrorResult& aRv)
{
  mCueList->RemoveCue(aCue, aRv);
  SetDirty();
}

void
TextTrack::UpdateActiveCueList()
{
  if (!mTextTrackList) {
    return;
  }

  HTMLMediaElement* mediaElement = mTextTrackList->GetMediaElement();
  if (!mediaElement) {
    return;
  }

  // If we are dirty, i.e. an event happened that may cause the sorted mCueList
  // to have changed like a seek or an insert for a cue, than we need to rebuild
  // the active cue list from scratch.
  if (mDirty) {
    mCuePos = 0;
    mDirty = false;
    mActiveCueList->RemoveAll();
  }

  double playbackTime = mediaElement->CurrentTime();
  // Remove all the cues from the active cue list whose end times now occur
  // earlier then the current playback time.
  for (uint32_t i = mActiveCueList->Length(); i > 0; i--) {
    if ((*mActiveCueList)[i - 1]->EndTime() < playbackTime) {
      mActiveCueList->RemoveCueAt(i - 1);
    }
  }
  // Add all the cues, starting from the position of the last cue that was
  // added, that have valid start and end times for the current playback time.
  // We can stop iterating safely once we encounter a cue that does not have
  // a valid start time as the cue list is sorted.
  for (; mCuePos < mCueList->Length() &&
         (*mCueList)[mCuePos]->StartTime() <= playbackTime; mCuePos++) {
    if ((*mCueList)[mCuePos]->EndTime() >= playbackTime) {
      mActiveCueList->AddCue(*(*mCueList)[mCuePos]);
    }
  }
}

TextTrackCueList*
TextTrack::GetActiveCues() {
  if (mMode != TextTrackMode::Disabled) {
    UpdateActiveCueList();
    return mActiveCueList;
  }
  return nullptr;
}

void
TextTrack::GetActiveCueArray(nsTArray<nsRefPtr<TextTrackCue> >& aCues)
{
  if (mMode != TextTrackMode::Disabled) {
    UpdateActiveCueList();
    mActiveCueList->GetArray(aCues);
  }
}

TextTrackReadyState
TextTrack::ReadyState() const
{
  return mReadyState;
}

void
TextTrack::SetReadyState(uint32_t aReadyState)
{
  if (aReadyState <= TextTrackReadyState::FailedToLoad) {
    SetReadyState(static_cast<TextTrackReadyState>(aReadyState));
  }
}

void
TextTrack::SetReadyState(TextTrackReadyState aState)
{
  mReadyState = aState;

  if (!mTextTrackList) {
    return;
  }

  HTMLMediaElement* mediaElement = mTextTrackList->GetMediaElement();
  if (mediaElement && (mReadyState == TextTrackReadyState::Loaded||
      mReadyState == TextTrackReadyState::FailedToLoad)) {
    mediaElement->RemoveTextTrack(this, true);
  }
}

TextTrackList*
TextTrack::GetTextTrackList()
{
  return mTextTrackList;
}

void
TextTrack::SetTextTrackList(TextTrackList* aTextTrackList)
{
  mTextTrackList = aTextTrackList;
}

HTMLTrackElement*
TextTrack::GetTrackElement() {
  return mTrackElement;
}

void
TextTrack::SetTrackElement(HTMLTrackElement* aTrackElement) {
  mTrackElement = aTrackElement;
}

} // namespace dom
} // namespace mozilla
