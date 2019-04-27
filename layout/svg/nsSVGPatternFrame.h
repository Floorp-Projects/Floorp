/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATTERNFRAME_H__
#define __NS_SVGPATTERNFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsAutoPtr.h"
#include "nsSVGPaintServerFrame.h"

class nsIFrame;

namespace mozilla {
class PresShell;
class SVGAnimatedLength;
class SVGAnimatedPreserveAspectRatio;
class SVGAnimatedTransformList;
class SVGAnimatedViewBox;
class SVGGeometryFrame;
}  // namespace mozilla

class nsSVGPatternFrame final : public nsSVGPaintServerFrame {
  typedef mozilla::gfx::SourceSurface SourceSurface;

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGPatternFrame)

  friend nsIFrame* NS_NewSVGPatternFrame(mozilla::PresShell* aPresShell,
                                         ComputedStyle* aStyle);

  explicit nsSVGPatternFrame(ComputedStyle* aStyle,
                             nsPresContext* aPresContext);

  // nsSVGPaintServerFrame methods:
  virtual already_AddRefed<gfxPattern> GetPaintServerPattern(
      nsIFrame* aSource, const DrawTarget* aDrawTarget,
      const gfxMatrix& aContextMatrix,
      nsStyleSVGPaint nsStyleSVG::*aFillOrStroke, float aGraphicOpacity,
      imgDrawingParams& aImgParams, const gfxRect* aOverrideBounds) override;

 public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio
      SVGAnimatedPreserveAspectRatio;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGPattern"), aResult);
  }
#endif  // DEBUG

 protected:
  /**
   * Parses this frame's href and - if it references another pattern - returns
   * it.  It also makes this frame a rendering observer of the specified ID.
   */
  nsSVGPatternFrame* GetReferencedPattern();

  // Accessors to lookup pattern attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent* aDefault);
  uint16_t GetEnumValue(uint32_t aIndex) {
    return GetEnumValue(aIndex, mContent);
  }
  mozilla::SVGAnimatedTransformList* GetPatternTransformList(
      nsIContent* aDefault);
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

  already_AddRefed<SourceSurface> PaintPattern(
      const DrawTarget* aDrawTarget, Matrix* patternMatrix,
      const Matrix& aContextMatrix, nsIFrame* aSource,
      nsStyleSVGPaint nsStyleSVG::*aFillOrStroke, float aGraphicOpacity,
      const gfxRect* aOverrideBounds, imgDrawingParams& aImgParams);

  /**
   * A <pattern> element may reference another <pattern> element using
   * xlink:href and, if it doesn't have any child content of its own, then it
   * will "inherit" the children of the referenced pattern (which may itself be
   * inheriting its children if it references another <pattern>).  This
   * function returns this nsSVGPatternFrame or the first pattern along the
   * reference chain (if there is one) to have children.
   */
  nsSVGPatternFrame* GetPatternWithChildren();

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
  mozilla::SVGGeometryFrame* mSource;
  nsAutoPtr<gfxMatrix> mCTM;

 protected:
  // This flag is used to detect loops in xlink:href processing
  bool mLoopFlag;
  bool mNoHRefURI;
};

#endif
