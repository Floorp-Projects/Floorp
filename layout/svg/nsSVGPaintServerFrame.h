/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPAINTSERVERFRAME_H__
#define __NS_SVGPAINTSERVERFRAME_H__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

class gfxContext;
class gfxPattern;
class nsStyleContext;

struct gfxRect;

/**
 * RAII class used to temporarily set and remove the
 * NS_FRAME_DRAWING_AS_PAINTSERVER frame state bit while a frame is being
 * drawn as a paint server.
 */
class MOZ_RAII AutoSetRestorePaintServerState
{
public:
  explicit AutoSetRestorePaintServerState(
             nsIFrame* aFrame
             MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
    mFrame(aFrame)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mFrame->AddStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
  }
  ~AutoSetRestorePaintServerState()
  {
    mFrame->RemoveStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
  }

private:
  nsIFrame* mFrame;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class nsSVGPaintServerFrame : public nsSVGContainerFrame
{
protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::image::DrawResult DrawResult;

  explicit nsSVGPaintServerFrame(nsStyleContext* aContext,
                                 mozilla::LayoutFrameType aType)
    : nsSVGContainerFrame(aContext, aType)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_ABSTRACT_FRAME(nsSVGPaintServerFrame)

  /**
   * Constructs a gfxPattern of the paint server rendering.
   *
   * @param aContextMatrix The transform matrix that is currently applied to
   *   the gfxContext that is being drawn to. This is needed by SVG patterns so
   *   that surfaces of the correct size can be created. (SVG gradients are
   *   vector based, so it's not used there.)
   */
  virtual mozilla::Pair<DrawResult, RefPtr<gfxPattern>>
    GetPaintServerPattern(nsIFrame *aSource,
                          const DrawTarget* aDrawTarget,
                          const gfxMatrix& aContextMatrix,
                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                          float aOpacity,
                          const gfxRect *aOverrideBounds = nullptr,
                          uint32_t aFlags = 0) = 0;

  // nsIFrame methods:
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsSVGContainerFrame::IsFrameOfType(aFlags & ~nsIFrame::eSVGPaintServer);
  }
};

#endif // __NS_SVGPAINTSERVERFRAME_H__
