/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SVGGEOMETRYFRAME_H__
#define __SVGGEOMETRYFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsFrame.h"
#include "nsSVGDisplayableFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGUtils.h"

namespace mozilla {
class SVGGeometryFrame;
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

class gfxContext;
class nsDisplaySVGGeometry;
class nsIAtom;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;
class nsSVGMarkerFrame;
class nsSVGMarkerProperty;

struct nsRect;

nsIFrame*
NS_NewSVGGeometryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

namespace mozilla {

class SVGGeometryFrame : public nsFrame
                       , public nsSVGDisplayableFrame
{
  typedef mozilla::gfx::DrawTarget DrawTarget;

  friend nsIFrame*
  ::NS_NewSVGGeometryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  friend class ::nsDisplaySVGGeometry;

protected:
  SVGGeometryFrame(nsStyleContext* aContext, mozilla::FrameType aType)
    : nsFrame(aContext, aType)
  {
     AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_MAY_BE_TRANSFORMED);
  }

  explicit SVGGeometryFrame(nsStyleContext* aContext)
    : SVGGeometryFrame(aContext, mozilla::FrameType::SVGGeometry)
  {}

public:
  NS_DECL_QUERYFRAME_TARGET(SVGGeometryFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame interface:
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGGeometry));
  }

  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual bool IsSVGTransformed(Matrix *aOwnTransforms = nullptr,
                                Matrix *aFromParentTransforms = nullptr) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGeometry"), aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  // SVGGeometryFrame methods
  gfxMatrix GetCanvasTM();
protected:
  // nsSVGDisplayableFrame interface:
  virtual DrawResult PaintSVG(gfxContext& aContext,
                              const gfxMatrix& aTransform,
                              const nsIntRect* aDirtyRect = nullptr,
                              uint32_t aFlags = 0) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  virtual SVGBBox GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                      uint32_t aFlags) override;
  virtual bool IsDisplayContainer() override { return false; }

  /**
   * This function returns a set of bit flags indicating which parts of the
   * element (fill, stroke, bounds) should intercept pointer events. It takes
   * into account the type of element and the value of the 'pointer-events'
   * property on the element.
   */
  virtual uint16_t GetHitTestFlags();
private:
  enum { eRenderFill = 1, eRenderStroke = 2 };
  DrawResult Render(gfxContext* aContext, uint32_t aRenderComponents,
                    const gfxMatrix& aTransform, uint32_t aFlags);

  /**
   * @param aMatrix The transform that must be multiplied onto aContext to
   *   establish this frame's SVG user space.
   */
  DrawResult PaintMarkers(gfxContext& aContext, const gfxMatrix& aMatrix,
                          uint32_t aFlags);

  struct MarkerProperties {
    nsSVGMarkerProperty* mMarkerStart;
    nsSVGMarkerProperty* mMarkerMid;
    nsSVGMarkerProperty* mMarkerEnd;

    bool MarkersExist() const {
      return mMarkerStart || mMarkerMid || mMarkerEnd;
    }

    nsSVGMarkerFrame *GetMarkerStartFrame();
    nsSVGMarkerFrame *GetMarkerMidFrame();
    nsSVGMarkerFrame *GetMarkerEndFrame();
  };

  /**
   * @param aFrame should be the first continuation
   */
  static MarkerProperties GetMarkerProperties(SVGGeometryFrame *aFrame);
};
} // namespace mozilla

#endif // __SVGGEOMETRYFRAME_H__
