/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTEXT_H_
#define MOZILLA_SVGCONTEXT_H_

#include "mozilla/Maybe.h"
#include "SVGPreserveAspectRatio.h"
#include "Units.h"

namespace mozilla {

// SVG image-specific rendering context. For imgIContainer::Draw.
// Used to pass information such as
//  - viewport information from CSS, and
//  - overridden attributes from an SVG <image> element
// to the image's internal SVG document when it's drawn.
class SVGImageContext
{
public:
  SVGImageContext()
    : mGlobalOpacity(1.0)
  { }

  // Note: 'aIsPaintingSVGImageElement' should be used to indicate whether
  // the SVG image in question is being painted for an SVG <image> element.
  SVGImageContext(CSSIntSize aViewportSize,
                  Maybe<SVGPreserveAspectRatio> aPreserveAspectRatio,
                  gfxFloat aOpacity = 1.0,
                  bool aIsPaintingSVGImageElement = false)
    : mViewportSize(aViewportSize)
    , mPreserveAspectRatio(aPreserveAspectRatio)
    , mGlobalOpacity(aOpacity)
    , mIsPaintingSVGImageElement(aIsPaintingSVGImageElement)
  { }

  const CSSIntSize& GetViewportSize() const {
    return mViewportSize;
  }

  const Maybe<SVGPreserveAspectRatio>& GetPreserveAspectRatio() const {
    return mPreserveAspectRatio;
  }

  gfxFloat GetGlobalOpacity() const {
    return mGlobalOpacity;
  }

  bool IsPaintingForSVGImageElement() const {
    return mIsPaintingSVGImageElement;
  }

  bool operator==(const SVGImageContext& aOther) const {
    return mViewportSize == aOther.mViewportSize &&
           mPreserveAspectRatio == aOther.mPreserveAspectRatio &&
           mGlobalOpacity == aOther.mGlobalOpacity &&
           mIsPaintingSVGImageElement == aOther.mIsPaintingSVGImageElement;
  }

  bool operator!=(const SVGImageContext& aOther) const {
    return !(*this == aOther);
  }

  uint32_t Hash() const {
    return HashGeneric(mViewportSize.width,
                       mViewportSize.height,
                       mPreserveAspectRatio.map(HashPAR).valueOr(0),
                       HashBytes(&mGlobalOpacity, sizeof(gfxFloat)),
                       mIsPaintingSVGImageElement);
  }

private:
  static uint32_t HashPAR(const SVGPreserveAspectRatio& aPAR) {
    return aPAR.Hash();
  }

  CSSIntSize                    mViewportSize;
  Maybe<SVGPreserveAspectRatio> mPreserveAspectRatio;
  gfxFloat                      mGlobalOpacity;
  bool                          mIsPaintingSVGImageElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGCONTEXT_H_
