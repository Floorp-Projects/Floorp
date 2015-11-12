/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGCLIPPATHFRAME_H__
#define __NS_SVGCLIPPATHFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;
class nsISVGChildFrame;

typedef nsSVGContainerFrame nsSVGClipPathFrameBase;

class nsSVGClipPathFrame : public nsSVGClipPathFrameBase
{
  friend nsIFrame*
  NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGClipPathFrame(nsStyleContext* aContext)
    : nsSVGClipPathFrameBase(aContext)
    , mInUse(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame methods:
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  // nsSVGClipPathFrame methods:

  /**
   * If the SVG clipPath is simple (as determined by the IsTrivial() method),
   * calling this method simply pushes a clip path onto the DrawTarget.  If the
   * SVG clipPath is not simple then calling this method will paint the
   * clipPath's contents (geometry being filled only, with opaque black) to the
   * DrawTarget.
   *
   * XXXjwatt Maybe split this into two methods.
   */
  nsresult ApplyClipOrPaintClipMask(gfxContext& aContext,
                                    nsIFrame* aClippedFrame,
                                    const gfxMatrix &aMatrix);

  /**
   * If the SVG clipPath is simple (as determined by the IsTrivial() method),
   * calling this method simply returns null.  If the SVG clipPath is not
   * simple then calling this method will return a mask surface containing
   * the clipped geometry. The reference context will be used to determine the
   * backend for the SourceSurface as well as the size, which will be limited
   * to the device clip extents on the context.
   */
  already_AddRefed<mozilla::gfx::SourceSurface>
    GetClipMask(gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
                const gfxMatrix& aMatrix, Matrix* aMaskTransform,
                mozilla::gfx::SourceSurface* aInputMask = nullptr,
                const mozilla::gfx::Matrix& aInputMaskTransform = mozilla::gfx::Matrix());

  /**
   * aPoint is expected to be in aClippedFrame's SVG user space.
   */
  bool PointIsInsideClipPath(nsIFrame* aClippedFrame, const gfxPoint &aPoint);

  // Check if this clipPath is made up of more than one geometry object.
  // If so, the clipping API in cairo isn't enough and we need to use
  // mask based clipping.
  bool IsTrivial(nsISVGChildFrame **aSingleChild = nullptr);

  bool IsValid();

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgClipPathFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGClipPath"), aResult);
  }
#endif

  SVGBBox 
  GetBBoxForClipPathFrame(const SVGBBox &aBBox, const gfxMatrix &aMatrix);

  /**
   * If the clipPath element transforms its children due to
   * clipPathUnits="objectBoundingBox" being set on it and/or due to the
   * 'transform' attribute being set on it, this function returns the resulting
   * transform.
   */
  gfxMatrix GetClipPathTransform(nsIFrame* aClippedFrame);

 private:
  // A helper class to allow us to paint clip paths safely. The helper
  // automatically sets and clears the mInUse flag on the clip path frame
  // (to prevent nasty reference loops). It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class MOZ_RAII AutoClipPathReferencer
  {
  public:
    explicit AutoClipPathReferencer(nsSVGClipPathFrame *aFrame
                                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
       : mFrame(aFrame) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      NS_ASSERTION(!mFrame->mInUse, "reference loop!");
      mFrame->mInUse = true;
    }
    ~AutoClipPathReferencer() {
      mFrame->mInUse = false;
    }
  private:
    nsSVGClipPathFrame *mFrame;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  gfxMatrix mMatrixForChildren;
  // recursion prevention flag
  bool mInUse;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

#endif
