/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridTrack.h"

#include "GridTracks.h"
#include "mozilla/dom/GridBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridTrack, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridTrack)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridTrack)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridTrack)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridTrack::GridTrack(GridTracks* aParent)
  : mParent(aParent)
  , mStart(0.0)
  , mBreadth(0.0)
  , mType(GridDeclaration::Implicit)
  , mState(GridTrackState::Static)
{
  MOZ_ASSERT(aParent, "Should never be instantiated with a null GridTracks");
}

GridTrack::~GridTrack()
{
}

JSObject*
GridTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridTrack_Binding::Wrap(aCx, this, aGivenProto);
}

double
GridTrack::Start() const
{
  return mStart;
}

double
GridTrack::Breadth() const
{
  return mBreadth;
}

GridDeclaration
GridTrack::Type() const
{
  return mType;
}

GridTrackState
GridTrack::State() const
{
  return mState;
}

void
GridTrack::SetTrackValues(double aStart,
                          double aBreadth,
                          GridDeclaration aType,
                          GridTrackState aState)
{
  mStart = aStart;
  mBreadth = aBreadth;
  mType = aType;
  mState = aState;
}

} // namespace dom
} // namespace mozilla
