/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGIMAGECONTEXT_H_
#define LAYOUT_SVG_SVGIMAGECONTEXT_H_

#include "mozilla/Maybe.h"
#include "mozilla/SVGContextPaint.h"
#include "mozilla/SVGPreserveAspectRatio.h"
#include "Units.h"

class nsIFrame;

namespace mozilla {

enum class ColorScheme : uint8_t;
class ComputedStyle;

// SVG image-specific rendering context. For imgIContainer::Draw.
// Used to pass information such as
//  - viewport and color-scheme information from CSS, and
//  - overridden attributes from an SVG <image> element
// to the image's internal SVG document when it's drawn.
class SVGImageContext {
 public:
  SVGImageContext() = default;

  /**
   * Currently it seems that the aViewportSize parameter ends up being used
   * for different things by different pieces of code, and probably in some
   * cases being used incorrectly (specifically in the case of pixel snapping
   * under the nsLayoutUtils::Draw*Image() methods).  An unfortunate result of
   * the messy code is that aViewportSize is currently a Maybe<T> since it
   * is difficult to create a utility function that consumers can use up
   * front to get the "correct" viewport size (i.e. which for compatibility
   * with the current code (bugs and all) would mean the size including all
   * the snapping and resizing magic that happens in various places under the
   * nsLayoutUtils::Draw*Image() methods on the way to DrawImageInternal
   * creating |fallbackContext|).  Using Maybe<T> allows code to pass Nothing()
   * in order to get the size that's created for |fallbackContext|.  At some
   * point we need to clean this code up, make our abstractions clear, create
   * that utility and stop using Maybe for this parameter.
   */
  explicit SVGImageContext(
      const Maybe<CSSIntSize>& aViewportSize,
      const Maybe<SVGPreserveAspectRatio>& aPreserveAspectRatio = Nothing(),
      const Maybe<ColorScheme>& aColorScheme = Nothing())
      : mViewportSize(aViewportSize),
        mPreserveAspectRatio(aPreserveAspectRatio),
        mColorScheme(aColorScheme) {}

  static void MaybeStoreContextPaint(SVGImageContext& aContext,
                                     nsIFrame* aFromFrame,
                                     imgIContainer* aImgContainer);

  static void MaybeStoreContextPaint(SVGImageContext& aContext,
                                     const nsPresContext&, const ComputedStyle&,
                                     imgIContainer*);

  const Maybe<CSSIntSize>& GetViewportSize() const { return mViewportSize; }

  void SetViewportSize(const Maybe<CSSIntSize>& aSize) {
    mViewportSize = aSize;
  }

  const Maybe<ColorScheme>& GetColorScheme() const { return mColorScheme; }

  void SetColorScheme(const Maybe<ColorScheme>& aScheme) {
    mColorScheme = aScheme;
  }

  const Maybe<SVGPreserveAspectRatio>& GetPreserveAspectRatio() const {
    return mPreserveAspectRatio;
  }

  void SetPreserveAspectRatio(const Maybe<SVGPreserveAspectRatio>& aPAR) {
    mPreserveAspectRatio = aPAR;
  }

  const SVGEmbeddingContextPaint* GetContextPaint() const {
    return mContextPaint.get();
  }

  void ClearContextPaint() { mContextPaint = nullptr; }

  bool operator==(const SVGImageContext& aOther) const {
    bool contextPaintIsEqual =
        // neither have context paint, or they have the same object:
        (mContextPaint == aOther.mContextPaint) ||
        // or both have context paint that are different but equivalent objects:
        (mContextPaint && aOther.mContextPaint &&
         *mContextPaint == *aOther.mContextPaint);

    return contextPaintIsEqual && mViewportSize == aOther.mViewportSize &&
           mPreserveAspectRatio == aOther.mPreserveAspectRatio &&
           mColorScheme == aOther.mColorScheme;
  }

  bool operator!=(const SVGImageContext& aOther) const {
    return !(*this == aOther);
  }

  PLDHashNumber Hash() const {
    PLDHashNumber hash = 0;
    if (mContextPaint) {
      hash = HashGeneric(hash, mContextPaint->Hash());
    }
    return HashGeneric(hash, mViewportSize.map(HashSize).valueOr(0),
                       mPreserveAspectRatio.map(HashPAR).valueOr(0),
                       mColorScheme.map(HashColorScheme).valueOr(0));
  }

 private:
  static PLDHashNumber HashSize(const CSSIntSize& aSize) {
    return HashGeneric(aSize.width, aSize.height);
  }
  static PLDHashNumber HashPAR(const SVGPreserveAspectRatio& aPAR) {
    return aPAR.Hash();
  }
  static PLDHashNumber HashColorScheme(ColorScheme aScheme) {
    return HashGeneric(uint8_t(aScheme));
  }

  // NOTE: When adding new member-vars, remember to update Hash() & operator==.
  RefPtr<SVGEmbeddingContextPaint> mContextPaint;
  Maybe<CSSIntSize> mViewportSize;
  Maybe<SVGPreserveAspectRatio> mPreserveAspectRatio;
  Maybe<ColorScheme> mColorScheme;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGIMAGECONTEXT_H_
