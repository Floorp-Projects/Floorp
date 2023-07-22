/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGMARKERFRAME_H_
#define LAYOUT_SVG_SVGMARKERFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"

class gfxContext;

namespace mozilla {

class PresShell;
class SVGGeometryFrame;

struct SVGMark;

namespace dom {
class SVGViewportElement;
}  // namespace dom
}  // namespace mozilla

nsContainerFrame* NS_NewSVGMarkerFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewSVGMarkerAnonChildFrame(mozilla::PresShell* aPresShell,
                                                mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGMarkerFrame final : public SVGContainerFrame {
  using imgDrawingParams = image::imgDrawingParams;

  friend class SVGMarkerAnonChildFrame;
  friend nsContainerFrame* ::NS_NewSVGMarkerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGMarkerFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGContainerFrame(aStyle, aPresContext, kClassID),
        mMarkedFrame(nullptr),
        mInUse(false),
        mInUse2(false) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY |
                 NS_STATE_SVG_RENDERING_OBSERVER_CONTAINER);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGMarkerFrame)

  // nsIFrame interface:
#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {}

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGMarker"_ns, aResult);
  }
#endif

  nsContainerFrame* GetContentInsertionFrame() override {
    // Any children must be added to our single anonymous inner frame kid.
    MOZ_ASSERT(
        PrincipalChildList().FirstChild() &&
            PrincipalChildList().FirstChild()->IsSVGMarkerAnonChildFrame(),
        "Where is our anonymous child?");
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  // SVGMarkerFrame methods:
  void PaintMark(gfxContext& aContext, const gfxMatrix& aToMarkedFrameUserSpace,
                 SVGGeometryFrame* aMarkedFrame, const SVGMark& aMark,
                 float aStrokeWidth, imgDrawingParams& aImgParams);

  SVGBBox GetMarkBBoxContribution(const Matrix& aToBBoxUserspace,
                                  uint32_t aFlags,
                                  SVGGeometryFrame* aMarkedFrame,
                                  const SVGMark& aMark, float aStrokeWidth);

  // Return our anonymous box child.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

 private:
  // stuff needed for callback
  SVGGeometryFrame* mMarkedFrame;
  Matrix mMarkerTM;

  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override;

  // A helper class to allow us to paint markers safely. The helper
  // automatically sets and clears the mInUse flag on the marker frame (to
  // prevent nasty reference loops) as well as the reference to the marked
  // frame and its coordinate context. It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class MOZ_RAII AutoMarkerReferencer {
   public:
    AutoMarkerReferencer(SVGMarkerFrame* aFrame,
                         SVGGeometryFrame* aMarkedFrame);
    ~AutoMarkerReferencer();

   private:
    SVGMarkerFrame* mFrame;
  };

  // SVGMarkerFrame methods:
  void SetParentCoordCtxProvider(dom::SVGViewportElement* aContext);

  // recursion prevention flag
  bool mInUse;

  // second recursion prevention flag, for GetCanvasTM()
  bool mInUse2;
};

////////////////////////////////////////////////////////////////////////
// nsMarkerAnonChildFrame class

class SVGMarkerAnonChildFrame final : public SVGDisplayContainerFrame {
  friend nsContainerFrame* ::NS_NewSVGMarkerAnonChildFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  explicit SVGMarkerAnonChildFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
      : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGMarkerAnonChildFrame)

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGMarkerAnonChild"_ns, aResult);
  }
#endif

  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override {
    return static_cast<SVGMarkerFrame*>(GetParent())->GetCanvasTM();
  }
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGMARKERFRAME_H_
