/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/VTTRegionBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TextTrackRegion, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackRegion)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackRegion)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackRegion)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
TextTrackRegion::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VTTRegionBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<TextTrackRegion>
TextTrackRegion::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TextTrackRegion> region = new TextTrackRegion(aGlobal.GetAsSupports());
  return region.forget();
}

TextTrackRegion::TextTrackRegion(nsISupports* aGlobal)
  : mParent(aGlobal)
  , mWidth(100)
  , mLines(3)
  , mRegionAnchorX(0)
  , mRegionAnchorY(100)
  , mViewportAnchorX(0)
  , mViewportAnchorY(100)
{
}

void
TextTrackRegion::CopyValues(TextTrackRegion& aRegion)
{
  mWidth = aRegion.Width();
  mLines = aRegion.Lines();
  mRegionAnchorX = aRegion.RegionAnchorX();
  mRegionAnchorY = aRegion.RegionAnchorY();
  mViewportAnchorX = aRegion.ViewportAnchorX();
  mViewportAnchorY = aRegion.ViewportAnchorY();
  mScroll = aRegion.Scroll();
}

} //namespace dom
} //namespace mozilla

