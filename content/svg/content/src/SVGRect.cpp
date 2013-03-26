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

//----------------------------------------------------------------------
// nsIDOMSVGRect methods:

/* attribute float x; */
NS_IMETHODIMP SVGRect::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP SVGRect::SetX(float aX)
{
  NS_ENSURE_FINITE(aX, NS_ERROR_ILLEGAL_VALUE);
  mX = aX;
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP SVGRect::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP SVGRect::SetY(float aY)
{
  NS_ENSURE_FINITE(aY, NS_ERROR_ILLEGAL_VALUE);
  mY = aY;
  return NS_OK;
}

/* attribute float width; */
NS_IMETHODIMP SVGRect::GetWidth(float *aWidth)
{
  *aWidth = mWidth;
  return NS_OK;
}
NS_IMETHODIMP SVGRect::SetWidth(float aWidth)
{
  NS_ENSURE_FINITE(aWidth, NS_ERROR_ILLEGAL_VALUE);
  mWidth = aWidth;
  return NS_OK;
}

/* attribute float height; */
NS_IMETHODIMP SVGRect::GetHeight(float *aHeight)
{
  *aHeight = mHeight;
  return NS_OK;
}
NS_IMETHODIMP SVGRect::SetHeight(float aHeight)
{
  NS_ENSURE_FINITE(aHeight, NS_ERROR_ILLEGAL_VALUE);
  mHeight = aHeight;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result, float x, float y,
              float width, float height)
{
  *result = new mozilla::dom::SVGRect(x, y, width, height);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result, const gfxRect& rect)
{
  return NS_NewSVGRect(result,
                       rect.X(), rect.Y(),
                       rect.Width(), rect.Height());
}

