/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGraphicsElement.h"
#include "mozilla/dom/SVGGraphicsElementBinding.h"

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)
NS_IMPL_RELEASE_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)

NS_INTERFACE_MAP_BEGIN(SVGGraphicsElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTests)
NS_INTERFACE_MAP_END_INHERITING(SVGGraphicsElementBase)

//----------------------------------------------------------------------
// Implementation

SVGGraphicsElement::SVGGraphicsElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGGraphicsElementBase(aNodeInfo)
{
}

} // namespace dom
} // namespace mozilla
