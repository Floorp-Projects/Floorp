/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGTransformableElement.h"
#include "DOMSVGAnimatedTransformList.h"

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGTransformableElement, SVGLocatableElement)
NS_IMPL_RELEASE_INHERITED(SVGTransformableElement, SVGLocatableElement)

NS_INTERFACE_MAP_BEGIN(SVGTransformableElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransformable)
NS_INTERFACE_MAP_END_INHERITING(SVGLocatableElement)


//----------------------------------------------------------------------
// nsIDOMSVGTransformable methods
/* readonly attribute nsISupports transform; */

NS_IMETHODIMP
SVGTransformableElement::GetTransform(nsISupports **aTransform)
{
  *aTransform = Transform().get();
  return NS_OK;
}

already_AddRefed<DOMSVGAnimatedTransformList>
SVGTransformableElement::Transform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return DOMSVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this).get();

}

} // namespace dom
} // namespace mozilla

