/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/TextTrackRegionList.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_5(TextTrack,
                                     nsDOMEventTargetHelper,
                                     mParent,
                                     mMediaElement,
                                     mCueList,
                                     mActiveCueList,
                                     mRegionList)

NS_IMPL_ADDREF_INHERITED(TextTrack, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrack)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrack::TextTrack(nsISupports* aParent)
  : mParent(aParent)
{
  SetDefaultSettings();
  SetIsDOMBinding();
}

TextTrack::TextTrack(nsISupports* aParent, HTMLMediaElement* aMediaElement)
  : mParent(aParent)
  , mMediaElement(aMediaElement)
{
  SetDefaultSettings();
  SetIsDOMBinding();
}

TextTrack::TextTrack(nsISupports* aParent,
                     HTMLMediaElement* aMediaElement,
                     TextTrackKind aKind,
                     const nsAString& aLabel,
                     const nsAString& aLanguage)
  : mParent(aParent)
  , mMediaElement(aMediaElement)
{
  SetDefaultSettings();
  mKind = aKind;
  mLabel = aLabel;
  mLanguage = aLanguage;
  SetIsDOMBinding();
}

void
TextTrack::SetDefaultSettings()
{
  mKind = TextTrackKind::Subtitles;
  mMode = TextTrackMode::Hidden;
  mCueList = new TextTrackCueList(mParent);
  mActiveCueList = new TextTrackCueList(mParent);
  mRegionList = new TextTrackRegionList(mParent);
  mCuePos = 0;
  mDirty = false;
  mReadyState = HTMLTrackElement::NONE;
}

JSObject*
TextTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackBinding::Wrap(aCx, aScope, this);
}

void
TextTrack::SetMode(TextTrackMode aValue)
{
  if (mMode != aValue) {
    mMode = aValue;
    if (mMediaElement) {
      mMediaElement->TextTracks()->CreateAndDispatchChangeEvent();
    }
  }
}

void
TextTrack::AddCue(TextTrackCue& aCue)
{
  mCueList->AddCue(aCue);
  if (mMediaElement) {
    mMediaElement->AddCue(aCue);
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
TextTrack::CueChanged(TextTrackCue& aCue)
{
  //XXX: Implement Cue changed. Bug 867823.
}

void
TextTrack::AddRegion(TextTrackRegion& aRegion)
{
  TextTrackRegion* region = mRegionList->GetRegionById(aRegion.Id());
  if (!region) {
    aRegion.SetTextTrack(this);
    mRegionList->AddTextTrackRegion(&aRegion);
    return;
  }

  region->CopyValues(aRegion);
}

void
TextTrack::RemoveRegion(const TextTrackRegion& aRegion, ErrorResult& aRv)
{
  if (!mRegionList->GetRegionById(aRegion.Id())) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  mRegionList->RemoveTextTrackRegion(aRegion);
}

void
TextTrack::UpdateActiveCueList()
{
  if (mMode == TextTrackMode::Disabled || !mMediaElement) {
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

  double playbackTime = mMediaElement->CurrentTime();
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
  UpdateActiveCueList();
  return mActiveCueList;
}

void
TextTrack::GetActiveCueArray(nsTArray<nsRefPtr<TextTrackCue> >& aCues)
{
  UpdateActiveCueList();
  mActiveCueList->GetArray(aCues);
}

uint16_t
TextTrack::ReadyState() const
{
  return mReadyState;
}

void
TextTrack::SetReadyState(uint16_t aState)
{
  mReadyState = aState;
  if (mMediaElement && (mReadyState == HTMLTrackElement::LOADED ||
      mReadyState == HTMLTrackElement::ERROR)) {
    mMediaElement->RemoveTextTrack(this, true);
  }
}

} // namespace dom
} // namespace mozilla
