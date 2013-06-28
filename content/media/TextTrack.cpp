/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/TextTrackRegionList.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_4(TextTrack,
                                     nsDOMEventTargetHelper,
                                     mParent,
                                     mCueList,
                                     mActiveCueList,
                                     mRegionList)

NS_IMPL_ADDREF_INHERITED(TextTrack, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrack)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrack::TextTrack(nsISupports* aParent,
                     TextTrackKind aKind,
                     const nsAString& aLabel,
                     const nsAString& aLanguage)
  : mParent(aParent)
  , mKind(aKind)
  , mLabel(aLabel)
  , mLanguage(aLanguage)
  , mMode(TextTrackMode::Hidden)
  , mCueList(new TextTrackCueList(aParent))
  , mActiveCueList(new TextTrackCueList(aParent))
  , mRegionList(new TextTrackRegionList(aParent))
{
  SetIsDOMBinding();
}

TextTrack::TextTrack(nsISupports* aParent)
  : mParent(aParent)
  , mKind(TextTrackKind::Subtitles)
  , mMode(TextTrackMode::Disabled)
  , mCueList(new TextTrackCueList(aParent))
  , mActiveCueList(new TextTrackCueList(aParent))
  , mRegionList(new TextTrackRegionList(aParent))
{
  SetIsDOMBinding();
}

void
TextTrack::Update(double aTime)
{
  mCueList->Update(aTime);
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
}

void
TextTrack::RemoveCue(TextTrackCue& aCue, ErrorResult& aRv)
{
  mCueList->RemoveCue(aCue, aRv);
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

} // namespace dom
} // namespace mozilla
