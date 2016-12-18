/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATHGEOMETRYFRAME_H__
#define __NS_SVGPATHGEOMETRYFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsFrame.h"
#include "nsISVGChildFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGUtils.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

class gfxContext;
class nsDisplaySVGPathGeometry;
class nsIAtom;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;
class nsSVGMarkerFrame;
class nsSVGMarkerProperty;

struct nsRect;

class nsSVGPathGeometryFrame : public nsFrame
                             , public nsISVGChildFrame
{
  typedef mozilla::gfx::DrawTarget DrawTarget;

  friend nsIFrame*
  NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  friend class nsDisplaySVGPathGeometry;

protected:
  explicit nsSVGPathGeometryFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
  {
     AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_MAY_BE_TRANSFORMED);
  }

public:
  NS_DECL_QUERYFRAME_TARGET(nsSVGPathGeometryFrame)
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

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgPathGeometryFrame
   */
  virtual nsIAtom* GetType() const override;

  virtual bool IsSVGTransformed(Matrix *aOwnTransforms = nullptr,
                                Matrix *aFromParentTransforms = nullptr) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPathGeometry"), aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  // nsSVGPathGeometryFrame methods
  gfxMatrix GetCanvasTM();
protected:
  // nsISVGChildFrame interface:
  virtual DrawResult PaintSVG(gfxContext& aContext,
                              const gfxMatrix& aTransform,
                              const nsIntRect* aDirtyRect = nullptr) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual nsRect GetCoveredRegion() override;
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
  void Render(gfxContext* aContext, uint32_t aRenderComponents,
              const gfxMatrix& aTransform);

  /**
   * @param aMatrix The transform that must be multiplied onto aContext to
   *   establish this frame's SVG user space.
   */
  void PaintMarkers(gfxContext& aContext, const gfxMatrix& aMatrix);

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
  static MarkerProperties GetMarkerProperties(nsSVGPathGeometryFrame *aFrame);
};

#endif // __NS_SVGPATHGEOMETRYFRAME_H__
