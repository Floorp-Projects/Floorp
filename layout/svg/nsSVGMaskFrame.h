/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;

typedef nsSVGContainerFrame nsSVGMaskFrameBase;

/**
 * Byte offsets of channels in a native packed gfxColor or cairo image surface.
 */
#ifdef IS_BIG_ENDIAN
#define GFX_ARGB32_OFFSET_A 0
#define GFX_ARGB32_OFFSET_R 1
#define GFX_ARGB32_OFFSET_G 2
#define GFX_ARGB32_OFFSET_B 3
#else
#define GFX_ARGB32_OFFSET_A 3
#define GFX_ARGB32_OFFSET_R 2
#define GFX_ARGB32_OFFSET_G 1
#define GFX_ARGB32_OFFSET_B 0
#endif

class nsSVGMaskFrame final : public nsSVGMaskFrameBase
{
  friend nsIFrame*
  NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;

protected:
  explicit nsSVGMaskFrame(nsStyleContext* aContext)
    : nsSVGMaskFrameBase(aContext)
    , mInUse(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsSVGMaskFrame method:
  already_AddRefed<SourceSurface>
  GetMaskForMaskedFrame(gfxContext* aContext,
                        nsIFrame* aMaskedFrame,
                        const gfxMatrix &aMatrix,
                        float aOpacity,
                        Matrix* aMaskTransform,
                        uint8_t aMaskOp = NS_STYLE_MASK_MODE_MATCH_SOURCE);

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMaskFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMask"), aResult);
  }
#endif

private:
  /**
   * If the mask element transforms its children due to
   * maskContentUnits="objectBoundingBox" being set on it, this function
   * returns the resulting transform.
   */
  gfxMatrix GetMaskTransform(nsIFrame* aMaskedFrame);

  // A helper class to allow us to paint masks safely. The helper
  // automatically sets and clears the mInUse flag on the mask frame
  // (to prevent nasty reference loops). It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class MOZ_RAII AutoMaskReferencer
  {
  public:
    explicit AutoMaskReferencer(nsSVGMaskFrame *aFrame
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
       : mFrame(aFrame) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      NS_ASSERTION(!mFrame->mInUse, "reference loop!");
      mFrame->mInUse = true;
    }
    ~AutoMaskReferencer() {
      mFrame->mInUse = false;
    }
  private:
    nsSVGMaskFrame *mFrame;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  gfxMatrix mMatrixForChildren;
  // recursion prevention flag
  bool mInUse;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

#endif
