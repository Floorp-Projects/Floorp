/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGRect_h
#define mozilla_dom_SVGRect_h

#include "gfxRect.h"
#include "mozilla/dom/SVGIRect.h"

////////////////////////////////////////////////////////////////////////
// SVGRect class

namespace mozilla {
namespace dom {

class SVGRect MOZ_FINAL : public SVGIRect
{
public:
  SVGRect(float x=0.0f, float y=0.0f, float w=0.0f, float h=0.0f);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // WebIDL
  virtual float X() const MOZ_OVERRIDE MOZ_FINAL
  {
    return mX;
  }

  virtual void SetX(float aX, ErrorResult& aRv) MOZ_FINAL
  {
    mX = aX;
  }

  virtual float Y() const MOZ_OVERRIDE MOZ_FINAL
  {
    return mY;
  }

  virtual void SetY(float aY, ErrorResult& aRv) MOZ_FINAL
  {
    mY = aY;
  }

  virtual float Width() const MOZ_OVERRIDE MOZ_FINAL
  {
    return mWidth;
  }

  virtual void SetWidth(float aWidth, ErrorResult& aRv) MOZ_FINAL
  {
    mWidth = aWidth;
  }

  virtual float Height() const MOZ_OVERRIDE MOZ_FINAL
  {
    return mHeight;
  }

  virtual void SetHeight(float aHeight, ErrorResult& aRv) MOZ_FINAL
  {
    mHeight = aHeight;
  }

  using mozilla::dom::SVGIRect::SetX;
  using mozilla::dom::SVGIRect::SetY;
  using mozilla::dom::SVGIRect::SetWidth;
  using mozilla::dom::SVGIRect::SetHeight;

protected:
  float mX, mY, mWidth, mHeight;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(float x=0.0f, float y=0.0f, float width=0.0f, float height=0.0f);

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(const gfxRect& rect);

#endif //mozilla_dom_SVGRect_h
