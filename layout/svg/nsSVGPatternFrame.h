/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATTERNFRAME_H__
#define __NS_SVGPATTERNFRAME_H__

#include "gfxMatrix.h"
#include "nsSVGPaintServerFrame.h"

class gfxASurface;
class gfxContext;
class nsIFrame;
class nsSVGElement;
class nsSVGLength2;
class nsSVGViewBox;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;
class nsSVGAnimatedTransformList;
} // namespace mozilla

typedef nsSVGPaintServerFrame  nsSVGPatternFrameBase;

/**
 * Patterns can refer to other patterns. We create an nsSVGPaintingProperty
 * with property type nsGkAtoms::href to track the referenced pattern.
 */
class nsSVGPatternFrame : public nsSVGPatternFrameBase
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewSVGPatternFrame(nsIPresShell* aPresShell,
                                         nsStyleContext* aContext);

  nsSVGPatternFrame(nsStyleContext* aContext);

  // nsSVGPaintServerFrame methods:
  virtual already_AddRefed<gfxPattern>
    GetPaintServerPattern(nsIFrame *aSource,
                          const gfxMatrix& aContextMatrix,
                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                          float aOpacity,
                          const gfxRect *aOverrideBounds);

public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor);

  // nsIFrame interface:
  NS_IMETHOD AttributeChanged(int32_t         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              int32_t         aModType);

#ifdef DEBUG
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgPatternFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPattern"), aResult);
  }
#endif // DEBUG

protected:
  // Internal methods for handling referenced patterns
  class AutoPatternReferencer;
  nsSVGPatternFrame* GetReferencedPattern();
  nsSVGPatternFrame* GetReferencedPatternIfNotInUse();

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

  nsresult PaintPattern(gfxASurface **surface,
                        gfxMatrix *patternMatrix,
                        const gfxMatrix &aContextMatrix,
                        nsIFrame *aSource,
                        nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                        float aGraphicOpacity,
                        const gfxRect *aOverrideBounds);
  nsIFrame*  GetPatternFirstChild();
  gfxRect    GetPatternRect(uint16_t aPatternUnits,
                            const gfxRect &bbox,
                            const gfxMatrix &callerCTM,
                            nsIFrame *aTarget);
  gfxMatrix  ConstructCTM(const nsSVGViewBox& aViewBox,
                          uint16_t aPatternContentUnits,
                          uint16_t aPatternUnits,
                          const gfxRect &callerBBox,
                          const gfxMatrix &callerCTM,
                          nsIFrame *aTarget);

private:
  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be temporary because different
  // referencing frames will all reference this one frame
  nsSVGGeometryFrame               *mSource;
  nsAutoPtr<gfxMatrix>              mCTM;

protected:
  // This flag is used to detect loops in xlink:href processing
  bool                              mLoopFlag;
  bool                              mNoHRefURI;
};

#endif
