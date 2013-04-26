/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGPreserveAspectRatio.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "mozilla/dom/SVGPreserveAspectRatioBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGPreserveAspectRatio, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPreserveAspectRatio)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPreserveAspectRatio)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPreserveAspectRatio)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

bool
SVGPreserveAspectRatio::operator==(const SVGPreserveAspectRatio& aOther) const
{
  return mAlign == aOther.mAlign &&
    mMeetOrSlice == aOther.mMeetOrSlice &&
    mDefer == aOther.mDefer;
}

JSObject*
DOMSVGPreserveAspectRatio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return mozilla::dom::SVGPreserveAspectRatioBinding::Wrap(aCx, aScope, this);
}

uint16_t
DOMSVGPreserveAspectRatio::Align()
{
  if (mIsBaseValue) {
    return mVal->GetBaseValue().GetAlign();
  }

  mSVGElement->FlushAnimations();
  return mVal->GetAnimValue().GetAlign();
}

void
DOMSVGPreserveAspectRatio::SetAlign(uint16_t aAlign, ErrorResult& rv)
{
  if (!mIsBaseValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->SetBaseAlign(aAlign, mSVGElement);
}

uint16_t
DOMSVGPreserveAspectRatio::MeetOrSlice()
{
  if (mIsBaseValue) {
    return mVal->GetBaseValue().GetMeetOrSlice();
  }

  mSVGElement->FlushAnimations();
  return mVal->GetAnimValue().GetMeetOrSlice();
}

void
DOMSVGPreserveAspectRatio::SetMeetOrSlice(uint16_t aMeetOrSlice, ErrorResult& rv)
{
  if (!mIsBaseValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->SetBaseMeetOrSlice(aMeetOrSlice, mSVGElement);
}

