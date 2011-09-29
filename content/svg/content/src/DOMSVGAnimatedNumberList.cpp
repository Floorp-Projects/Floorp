/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "DOMSVGAnimatedNumberList.h"
#include "DOMSVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"

// See the architecture comment in this file's header.

namespace mozilla {

static nsSVGAttrTearoffTable<SVGAnimatedNumberList, DOMSVGAnimatedNumberList>
  sSVGAnimatedNumberListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGAnimatedNumberList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGAnimatedNumberList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGAnimatedNumberList)

} // namespace mozilla
DOMCI_DATA(SVGAnimatedNumberList, mozilla::DOMSVGAnimatedNumberList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGAnimatedNumberList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedNumberList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedNumberList)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
DOMSVGAnimatedNumberList::GetBaseVal(nsIDOMSVGNumberList **_retval)
{
  if (!mBaseVal) {
    mBaseVal = new DOMSVGNumberList(this, InternalAList().GetBaseValue());
  }
  NS_ADDREF(*_retval = mBaseVal);
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGAnimatedNumberList::GetAnimVal(nsIDOMSVGNumberList **_retval)
{
  if (!mAnimVal) {
    mAnimVal = new DOMSVGNumberList(this, InternalAList().GetAnimValue());
  }
  NS_ADDREF(*_retval = mAnimVal);
  return NS_OK;
}

/* static */ already_AddRefed<DOMSVGAnimatedNumberList>
DOMSVGAnimatedNumberList::GetDOMWrapper(SVGAnimatedNumberList *aList,
                                        nsSVGElement *aElement,
                                        PRUint8 aAttrEnum)
{
  DOMSVGAnimatedNumberList *wrapper =
    sSVGAnimatedNumberListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedNumberList(aElement, aAttrEnum);
    sSVGAnimatedNumberListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

/* static */ DOMSVGAnimatedNumberList*
DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(SVGAnimatedNumberList *aList)
{
  return sSVGAnimatedNumberListTearoffTable.GetTearoff(aList);
}

DOMSVGAnimatedNumberList::~DOMSVGAnimatedNumberList()
{
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  sSVGAnimatedNumberListTearoffTable.RemoveTearoff(&InternalAList());
}

void
DOMSVGAnimatedNumberList::InternalBaseValListWillChangeTo(const SVGNumberList& aNewValue)
{
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  nsRefPtr<DOMSVGAnimatedNumberList> kungFuDeathGrip;
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
DOMSVGAnimatedNumberList::InternalAnimValListWillChangeTo(const SVGNumberList& aNewValue)
{
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewValue.Length());
  }
}

bool
DOMSVGAnimatedNumberList::IsAnimating() const
{
  return InternalAList().IsAnimating();
}

SVGAnimatedNumberList&
DOMSVGAnimatedNumberList::InternalAList()
{
  return *mElement->GetAnimatedNumberList(mAttrEnum);
}

const SVGAnimatedNumberList&
DOMSVGAnimatedNumberList::InternalAList() const
{
  return *mElement->GetAnimatedNumberList(mAttrEnum);
}

} // namespace mozilla
