/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsSVGLength2;
class nsSVGViewBox;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;
class SVGGeometryFrame;
class nsSVGAnimatedTransformList;
} // namespace mozilla

/**
 * Patterns can refer to other patterns. We create an nsSVGPaintingProperty
 * with property type nsGkAtoms::href to track the referenced pattern.
 */
class nsSVGPatternFrame final : public nsSVGPaintServerFrame
{
  typedef mozilla::gfx::SourceSurface SourceSurface;

public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewSVGPatternFrame(nsIPresShell* aPresShell,
                                         nsStyleContext* aContext);

  explicit nsSVGPatternFrame(nsStyleContext* aContext);

  // nsSVGPaintServerFrame methods:
  virtual mozilla::Pair<DrawResult, RefPtr<gfxPattern>>
    GetPaintServerPattern(nsIFrame *aSource,
                          const DrawTarget* aDrawTarget,
                          const gfxMatrix& aContextMatrix,
                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                          float aOpacity,
                          const gfxRect *aOverrideBounds,
                          uint32_t aFlags) override;

public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPattern"), aResult);
  }
#endif // DEBUG

protected:
  // Internal methods for handling referenced patterns
  nsSVGPatternFrame* GetReferencedPattern();

  // Accessors to lookup pattern attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent *aDefault);
  uint16_t GetEnumValue(uint32_t aIndex)
  {
    return GetEnumValue(aIndex, mContent);
  }
  mozilla::nsSVGAnimatedTransformList* GetPatternTransformList(
      nsIContent* aDefault);
  gfxMatrix GetPatternTransform();
  const nsSVGViewBox &GetViewBox(nsIContent *aDefault);
  const nsSVGViewBox &GetViewBox() { return GetViewBox(mContent); }
  const SVGAnimatedPreserveAspectRatio &GetPreserveAspectRatio(
      nsIContent *aDefault);
  const SVGAnimatedPreserveAspectRatio &GetPreserveAspectRatio()
  {
    return GetPreserveAspectRatio(mContent);
  }
  const nsSVGLength2 *GetLengthValue(uint32_t aIndex, nsIContent *aDefault);
  const nsSVGLength2 *GetLengthValue(uint32_t aIndex)
  {
    return GetLengthValue(aIndex, mContent);
  }

  mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
  PaintPattern(const DrawTarget* aDrawTarget,
               Matrix *patternMatrix,
               const Matrix &aContextMatrix,
               nsIFrame *aSource,
               nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
               float aGraphicOpacity,
               const gfxRect *aOverrideBounds,
               uint32_t aFlags);

  /**
   * A <pattern> element may reference another <pattern> element using
   * xlink:href and, if it doesn't have any child content of its own, then it
   * will "inherit" the children of the referenced pattern (which may itself be
   * inheriting its children if it references another <pattern>).  This
   * function returns this nsSVGPatternFrame or the first pattern along the
   * reference chain (if there is one) to have children.
   */
  nsSVGPatternFrame* GetPatternWithChildren();

  gfxRect    GetPatternRect(uint16_t aPatternUnits,
                            const gfxRect &bbox,
                            const Matrix &callerCTM,
                            nsIFrame *aTarget);
  gfxMatrix  ConstructCTM(const nsSVGViewBox& aViewBox,
                          uint16_t aPatternContentUnits,
                          uint16_t aPatternUnits,
                          const gfxRect &callerBBox,
                          const Matrix &callerCTM,
                          nsIFrame *aTarget);

private:
  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be temporary because different
  // referencing frames will all reference this one frame
  mozilla::SVGGeometryFrame        *mSource;
  nsAutoPtr<gfxMatrix>              mCTM;

protected:
  // This flag is used to detect loops in xlink:href processing
  bool                              mLoopFlag;
  bool                              mNoHRefURI;
};

#endif
