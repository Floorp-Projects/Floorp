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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "DOMSVGAnimatedTransformList.h"
#include "DOMSVGTransformList.h"
#include "SVGAnimatedTransformList.h"
#include "nsSVGAttrTearoffTable.h"

namespace mozilla {

static
  nsSVGAttrTearoffTable<SVGAnimatedTransformList,DOMSVGAnimatedTransformList>
  sSVGAnimatedTransformListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGAnimatedTransformList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGAnimatedTransformList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGAnimatedTransformList)

} // namespace mozilla
DOMCI_DATA(SVGAnimatedTransformList, mozilla::DOMSVGAnimatedTransformList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGAnimatedTransformList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedTransformList)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedTransformList methods:

/* readonly attribute nsIDOMSVGTransformList baseVal; */
NS_IMETHODIMP
DOMSVGAnimatedTransformList::GetBaseVal(nsIDOMSVGTransformList** aBaseVal)
{
  if (!mBaseVal) {
    mBaseVal = new DOMSVGTransformList(this, InternalAList().GetBaseValue());
  }
  NS_ADDREF(*aBaseVal = mBaseVal);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGTransformList animVal; */
NS_IMETHODIMP
DOMSVGAnimatedTransformList::GetAnimVal(nsIDOMSVGTransformList** aAnimVal)
{
  if (!mAnimVal) {
    mAnimVal = new DOMSVGTransformList(this, InternalAList().GetAnimValue());
  }
  NS_ADDREF(*aAnimVal = mAnimVal);
  return NS_OK;
}

/* static */ already_AddRefed<DOMSVGAnimatedTransformList>
DOMSVGAnimatedTransformList::GetDOMWrapper(SVGAnimatedTransformList *aList,
                                           nsSVGElement *aElement)
{
  DOMSVGAnimatedTransformList *wrapper =
    sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedTransformList(aElement);
    sSVGAnimatedTransformListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

/* static */ DOMSVGAnimatedTransformList*
DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(
  SVGAnimatedTransformList *aList)
{
  return sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
}

DOMSVGAnimatedTransformList::~DOMSVGAnimatedTransformList()
{
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  sSVGAnimatedTransformListTearoffTable.RemoveTearoff(&InternalAList());
}

void
DOMSVGAnimatedTransformList::InternalBaseValListWillChangeLengthTo(
  PRUint32 aNewLength)
{
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  nsRefPtr<DOMSVGAnimatedTransformList> kungFuDeathGrip;
  if (mBaseVal) {
    if (aNewLength < mBaseVal->Length()) {
      // InternalListLengthWillChange might clear last reference to |this|.
      // Retain a temporary reference to keep from dying before returning.
      kungFuDeathGrip = this;
    }
    mBaseVal->InternalListLengthWillChange(aNewLength);
  }

  // If our attribute is not animating, then our animVal mirrors our baseVal
  // and we must sync its length too. (If our attribute is animating, then the
  // SMIL engine takes care of calling InternalAnimValListWillChangeLengthTo()
  // if necessary.)

  if (!IsAnimating()) {
    InternalAnimValListWillChangeLengthTo(aNewLength);
  }
}

void
DOMSVGAnimatedTransformList::InternalAnimValListWillChangeLengthTo(
  PRUint32 aNewLength)
{
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewLength);
  }
}

PRBool
DOMSVGAnimatedTransformList::IsAnimating() const
{
  return InternalAList().IsAnimating();
}

SVGAnimatedTransformList&
DOMSVGAnimatedTransformList::InternalAList()
{
  return *mElement->GetAnimatedTransformList();
}

const SVGAnimatedTransformList&
DOMSVGAnimatedTransformList::InternalAList() const
{
  return *mElement->GetAnimatedTransformList();
}

} // namespace mozilla
