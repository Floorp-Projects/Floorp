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

void
TextTrack::Update(double aTime)
{
  if (mCueList) {
    mCueList->Update(aTime);
  }
}

JSObject*
TextTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackBinding::Wrap(aCx, aScope, this);
}

void
TextTrack::SetMode(TextTrackMode aValue)
{
  mMode = aValue;
}

void
TextTrack::AddCue(TextTrackCue& aCue)
{
  mCueList->AddCue(aCue);
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

TextTrackCueList*
TextTrack::GetActiveCues()
{
  if (mMode == TextTrackMode::Disabled || !mMediaElement) {
    return nullptr;
  }

  // If we are dirty, i.e. an event happened that may cause the sorted mCueList
  // to have changed like a seek or an insert for a cue, than we need to rebuild
  // the active cue list from scratch.
  if (mDirty) {
    mCuePos = 0;
    mDirty = true;
    mActiveCueList->RemoveAll();
  }

  double playbackTime = mMediaElement->CurrentTime();
  // Remove all the cues from the active cue list whose end times now occur
  // earlier then the current playback time. When we reach a cue whose end time
  // is valid we can safely stop iterating as the list is sorted.
  for (uint32_t i = 0; i < mActiveCueList->Length() &&
                       (*mActiveCueList)[i]->EndTime() < playbackTime; i++) {
    mActiveCueList->RemoveCueAt(i);
  }
  // Add all the cues, starting from the position of the last cue that was
  // added, that have valid start and end times for the current playback time.
  // We can stop iterating safely once we encounter a cue that does not have
  // valid times for the current playback time as the cue list is sorted.
  for (; mCuePos < mCueList->Length(); mCuePos++) {
    TextTrackCue* cue = (*mCueList)[mCuePos];
    if (cue->StartTime() > playbackTime || cue->EndTime() < playbackTime) {
      break;
    }
    mActiveCueList->AddCue(*cue);
  }
  return mActiveCueList;
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
