/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Pair.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;

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

class nsSVGMaskFrame final : public nsSVGContainerFrame
{
  friend nsIFrame*
  NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::image::DrawResult DrawResult;

protected:
  explicit nsSVGMaskFrame(nsStyleContext* aContext)
    : nsSVGContainerFrame(aContext, mozilla::FrameType::SVGMask)
    , mInUse(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  struct MaskParams {
    gfxContext* ctx;
    nsIFrame* maskedFrame;
    const gfxMatrix& toUserSpace;
    float opacity;
    Matrix* maskTransform;
    uint8_t maskMode;
    uint32_t flags;  // Image flags of the imgIContainer::FLAG_* variety.

    explicit MaskParams(gfxContext* aCtx, nsIFrame* aMaskedFrame,
                        const gfxMatrix& aToUserSpace, float aOpacity,
                        Matrix* aMaskTransform, uint8_t aMaskMode,
                        uint32_t aFlags)
    : ctx(aCtx), maskedFrame(aMaskedFrame), toUserSpace(aToUserSpace),
      opacity(aOpacity), maskTransform(aMaskTransform), maskMode(aMaskMode),
      flags(aFlags)
    { }
  };

  // nsSVGMaskFrame method:
  mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
  GetMaskForMaskedFrame(MaskParams& aParams);

  gfxRect
  GetMaskArea(nsIFrame* aMaskedFrame);

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

  gfxMatrix mMatrixForChildren;
  // recursion prevention flag
  bool mInUse;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

#endif
