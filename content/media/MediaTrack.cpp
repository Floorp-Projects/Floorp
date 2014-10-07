/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrack.h"
#include "MediaTrackList.h"

namespace mozilla {
namespace dom {

MediaTrack::MediaTrack(const nsAString& aId,
                       const nsAString& aKind,
                       const nsAString& aLabel,
                       const nsAString& aLanguage)
  : DOMEventTargetHelper()
  , mId(aId)
  , mKind(aKind)
  , mLabel(aLabel)
  , mLanguage(aLanguage)
{
}

MediaTrack::~MediaTrack()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaTrack, DOMEventTargetHelper, mList)

NS_IMPL_ADDREF_INHERITED(MediaTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaTrack::SetTrackList(MediaTrackList* aList)
{
  mList = aList;
}

void
MediaTrack::Init(nsPIDOMWindow* aOwnerWindow)
{
  BindToOwner(aOwnerWindow);
}

} // namespace dom
} // namespace mozilla
