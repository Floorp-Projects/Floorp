/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMARKERFRAME_H__
#define __NS_SVGMARKERFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;
class nsSVGPathGeometryFrame;

namespace mozilla {
namespace dom {
class SVGSVGElement;
} // namespace dom
} // namespace mozilla

struct nsSVGMark;

typedef nsSVGContainerFrame nsSVGMarkerFrameBase;

class nsSVGMarkerFrame : public nsSVGMarkerFrameBase
{
  friend class nsSVGMarkerAnonChildFrame;
  friend nsContainerFrame*
  NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGMarkerFrame(nsStyleContext* aContext)
    : nsSVGMarkerFrameBase(aContext)
    , mMarkedFrame(nullptr)
    , mInUse(false)
    , mInUse2(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;
  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMarkerFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMarker"), aResult);
  }
#endif

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    // Any children must be added to our single anonymous inner frame kid.
    MOZ_ASSERT(PrincipalChildList().FirstChild() &&
               PrincipalChildList().FirstChild()->GetType() ==
                 nsGkAtoms::svgMarkerAnonChildFrame,
               "Where is our anonymous child?");
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  // nsSVGMarkerFrame methods:
  nsresult PaintMark(gfxContext& aContext,
                     const gfxMatrix& aToMarkedFrameUserSpace,
                     nsSVGPathGeometryFrame *aMarkedFrame,
                     nsSVGMark *aMark,
                     float aStrokeWidth);

  SVGBBox GetMarkBBoxContribution(const Matrix &aToBBoxUserspace,
                                  uint32_t aFlags,
                                  nsSVGPathGeometryFrame *aMarkedFrame,
                                  const nsSVGMark *aMark,
                                  float aStrokeWidth);

private:
  // stuff needed for callback
  nsSVGPathGeometryFrame *mMarkedFrame;
  float mStrokeWidth, mX, mY, mAutoAngle;
  bool mIsStart;  // whether the callback is for a marker-start marker

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  // A helper class to allow us to paint markers safely. The helper
  // automatically sets and clears the mInUse flag on the marker frame (to
  // prevent nasty reference loops) as well as the reference to the marked
  // frame and its coordinate context. It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class MOZ_RAII AutoMarkerReferencer
  {
  public:
    AutoMarkerReferencer(nsSVGMarkerFrame *aFrame,
                         nsSVGPathGeometryFrame *aMarkedFrame
                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoMarkerReferencer();
  private:
    nsSVGMarkerFrame *mFrame;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  // nsSVGMarkerFrame methods:
  void SetParentCoordCtxProvider(mozilla::dom::SVGSVGElement *aContext);

  // recursion prevention flag
  bool mInUse;

  // second recursion prevention flag, for GetCanvasTM()
  bool mInUse2;
};

////////////////////////////////////////////////////////////////////////
// nsMarkerAnonChildFrame class

typedef nsSVGDisplayContainerFrame nsSVGMarkerAnonChildFrameBase;

/**
 */
class nsSVGMarkerAnonChildFrame
  : public nsSVGMarkerAnonChildFrameBase
{
  friend nsContainerFrame*
  NS_NewSVGMarkerAnonChildFrame(nsIPresShell* aPresShell,
                                nsStyleContext* aContext);

  explicit nsSVGMarkerAnonChildFrame(nsStyleContext* aContext)
    : nsSVGMarkerAnonChildFrameBase(aContext)
  {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGMarkerAnonChild"), aResult);
  }
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMarkerAnonChildFrame
   */
  virtual nsIAtom* GetType() const override;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override
  {
    return static_cast<nsSVGMarkerFrame*>(GetParent())->GetCanvasTM();
  }
};
#endif
