/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGRect.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

DOMCI_DATA(SVGRect, mozilla::dom::SVGRect)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// implementation:

SVGRect::SVGRect(float x, float y, float w, float h)
    : mX(x), mY(y), mWidth(w), mHeight(h)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(SVGRect)
NS_IMPL_RELEASE(SVGRect)

NS_INTERFACE_MAP_BEGIN(SVGRect)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGRect)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGRect)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(float x, float y, float width, float height)
{
  nsRefPtr<mozilla::dom::SVGRect> rect =
    new mozilla::dom::SVGRect(x, y, width, height);

  return rect.forget();
}

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(const gfxRect& rect)
{
  return NS_NewSVGRect(rect.X(), rect.Y(),
                       rect.Width(), rect.Height());
}

