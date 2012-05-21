/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedLengthList.h"
#include "DOMSVGLengthList.h"
#include "SVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"

// See the architecture comment in this file's header.

namespace mozilla {

static nsSVGAttrTearoffTable<SVGAnimatedLengthList, DOMSVGAnimatedLengthList>
  sSVGAnimatedLengthListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGAnimatedLengthList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGAnimatedLengthList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGAnimatedLengthList)

} // namespace mozilla
DOMCI_DATA(SVGAnimatedLengthList, mozilla::DOMSVGAnimatedLengthList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGAnimatedLengthList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedLengthList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedLengthList)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
DOMSVGAnimatedLengthList::GetBaseVal(nsIDOMSVGLengthList **_retval)
{
  if (!mBaseVal) {
    mBaseVal = new DOMSVGLengthList(this, InternalAList().GetBaseValue());
  }
  NS_ADDREF(*_retval = mBaseVal);
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGAnimatedLengthList::GetAnimVal(nsIDOMSVGLengthList **_retval)
{
  if (!mAnimVal) {
    mAnimVal = new DOMSVGLengthList(this, InternalAList().GetAnimValue());
  }
  NS_ADDREF(*_retval = mAnimVal);
  return NS_OK;
}

/* static */ already_AddRefed<DOMSVGAnimatedLengthList>
DOMSVGAnimatedLengthList::GetDOMWrapper(SVGAnimatedLengthList *aList,
                                        nsSVGElement *aElement,
                                        PRUint8 aAttrEnum,
                                        PRUint8 aAxis)
{
  DOMSVGAnimatedLengthList *wrapper =
    sSVGAnimatedLengthListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedLengthList(aElement, aAttrEnum, aAxis);
    sSVGAnimatedLengthListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

/* static */ DOMSVGAnimatedLengthList*
DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(SVGAnimatedLengthList *aList)
{
  return sSVGAnimatedLengthListTearoffTable.GetTearoff(aList);
}

DOMSVGAnimatedLengthList::~DOMSVGAnimatedLengthList()
{
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  sSVGAnimatedLengthListTearoffTable.RemoveTearoff(&InternalAList());
}

void
DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo(const SVGLengthList& aNewValue)
{
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  nsRefPtr<DOMSVGAnimatedLengthList> kungFuDeathGrip;
  if (mBaseVal) {
    if (aNewValue.Length() < mBaseVal->Length()) {
      // InternalListLengthWillChange might clear last reference to |this|.
      // Retain a temporary reference to keep from dying before returning.
      kungFuDeathGrip = this;
    }
    mBaseVal->InternalListLengthWillChange(aNewValue.Length());
  }

  // If our attribute is not animating, then our animVal mirrors our baseVal
  // and we must sync its length too. (If our attribute is animating, then the
  // SMIL engine takes care of calling InternalAnimValListWillChangeTo() if
  // necessary.)

  if (!IsAnimating()) {
    InternalAnimValListWillChangeTo(aNewValue);
  }
}

void
DOMSVGAnimatedLengthList::InternalAnimValListWillChangeTo(const SVGLengthList& aNewValue)
{
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewValue.Length());
  }
}

bool
DOMSVGAnimatedLengthList::IsAnimating() const
{
  return InternalAList().IsAnimating();
}

SVGAnimatedLengthList&
DOMSVGAnimatedLengthList::InternalAList()
{
  return *mElement->GetAnimatedLengthList(mAttrEnum);
}

const SVGAnimatedLengthList&
DOMSVGAnimatedLengthList::InternalAList() const
{
  return *mElement->GetAnimatedLengthList(mAttrEnum);
}

} // namespace mozilla
