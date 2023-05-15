/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGPATTERNFRAME_H_
#define LAYOUT_SVG_SVGPATTERNFRAME_H_

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGPaintServerFrame.h"
#include "mozilla/UniquePtr.h"

class nsIFrame;

namespace mozilla {
class PresShell;
class SVGAnimatedLength;
class SVGAnimatedPreserveAspectRatio;
class SVGAnimatedTransformList;
class SVGAnimatedViewBox;
class SVGGeometryFrame;
}  // namespace mozilla

nsIFrame* NS_NewSVGPatternFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGPatternFrame final : public SVGPaintServerFrame {
  using SourceSurface = gfx::SourceSurface;

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGPatternFrame)
  NS_DECL_QUERYFRAME

  friend nsIFrame* ::NS_NewSVGPatternFrame(mozilla::PresShell* aPresShell,
                                           ComputedStyle* aStyle);

  explicit SVGPatternFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  // SVGPaintServerFrame methods:
  already_AddRefed<gfxPattern> GetPaintServerPattern(
      nsIFrame* aSource, const DrawTarget* aDrawTarget,
      const gfxMatrix& aContextMatrix, StyleSVGPaint nsStyleSVG::*aFillOrStroke,
      float aGraphicOpacity, imgDrawingParams& aImgParams,
      const gfxRect* aOverrideBounds) override;

 public:
  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override;

  // nsIFrame interface:
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGPattern"_ns, aResult);
  }
#endif  // DEBUG

 protected:
  /**
   * Parses this frame's href and - if it references another pattern - returns
   * it.  It also makes this frame a rendering observer of the specified ID.
   */
  SVGPatternFrame* GetReferencedPattern();

  // Accessors to lookup pattern attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent* aDefault);
  uint16_t GetEnumValue(uint32_t aIndex) {
    return GetEnumValue(aIndex, mContent);
  }
  SVGAnimatedTransformList* GetPatternTransformList(nsIContent* aDefault);
  gfxMatrix GetPatternTransform();
  const SVGAnimatedViewBox& GetViewBox(nsIContent* aDefault);
  const SVGAnimatedViewBox& GetViewBox() { return GetViewBox(mContent); }
  const SVGAnimatedPreserveAspectRatio& GetPreserveAspectRatio(
      nsIContent* aDefault);
  const SVGAnimatedPreserveAspectRatio& GetPreserveAspectRatio() {
    return GetPreserveAspectRatio(mContent);
  }
  const SVGAnimatedLength* GetLengthValue(uint32_t aIndex,
                                          nsIContent* aDefault);
  const SVGAnimatedLength* GetLengthValue(uint32_t aIndex) {
    return GetLengthValue(aIndex, mContent);
  }

  void PaintChildren(DrawTarget* aDrawTarget,
                     SVGPatternFrame* aPatternWithChildren, nsIFrame* aSource,
                     float aGraphicOpacity, imgDrawingParams& aImgParams);

  already_AddRefed<SourceSurface> PaintPattern(
      const DrawTarget* aDrawTarget, Matrix* patternMatrix,
      const Matrix& aContextMatrix, nsIFrame* aSource,
      StyleSVGPaint nsStyleSVG::*aFillOrStroke, float aGraphicOpacity,
      const gfxRect* aOverrideBounds, imgDrawingParams& aImgParams);

  /**
   * A <pattern> element may reference another <pattern> element using
   * xlink:href and, if it doesn't have any child content of its own, then it
   * will "inherit" the children of the referenced pattern (which may itself be
   * inheriting its children if it references another <pattern>).  This
   * function returns this SVGPatternFrame or the first pattern along the
   * reference chain (if there is one) to have children.
   */
  SVGPatternFrame* GetPatternWithChildren();

  gfxRect GetPatternRect(uint16_t aPatternUnits, const gfxRect& bbox,
                         const Matrix& aTargetCTM, nsIFrame* aTarget);
  gfxMatrix ConstructCTM(const SVGAnimatedViewBox& aViewBox,
                         uint16_t aPatternContentUnits, uint16_t aPatternUnits,
                         const gfxRect& callerBBox, const Matrix& callerCTM,
                         nsIFrame* aTarget);

 private:
  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be temporary because different
  // referencing frames will all reference this one frame
  SVGGeometryFrame* mSource;
  UniquePtr<gfxMatrix> mCTM;

 protected:
  // This flag is used to detect loops in xlink:href processing
  bool mLoopFlag;
  bool mNoHRefURI;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGPATTERNFRAME_H_
