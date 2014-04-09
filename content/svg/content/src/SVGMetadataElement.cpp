/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMetadataElement.h"
#include "mozilla/dom/SVGMetadataElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Metadata)

namespace mozilla {
namespace dom {

JSObject*
SVGMetadataElement::WrapNode(JSContext *aCx)
{
  return SVGMetadataElementBinding::Wrap(aCx, this);
}

//----------------------------------------------------------------------
// Implementation

SVGMetadataElement::SVGMetadataElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGMetadataElementBase(aNodeInfo)
{
}


nsresult
SVGMetadataElement::Init()
{
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMetadataElement)

} // namespace dom
} // namespace mozilla

