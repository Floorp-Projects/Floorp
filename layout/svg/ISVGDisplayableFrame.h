/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_ISVGDISPLAYABLEFRAME_H_
#define LAYOUT_SVG_ISVGDISPLAYABLEFRAME_H_

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsQueryFrame.h"
#include "nsRect.h"
#include "mozilla/gfx/MatrixFwd.h"

class gfxContext;
class nsIFrame;

namespace mozilla {
class SVGAnimatedLengthList;
class SVGAnimatedNumberList;
class SVGBBox;
class SVGLengthList;
class SVGNumberList;
class SVGUserUnitList;

namespace image {
struct imgDrawingParams;
}  // namespace image

/**
 * This class is used for elements that can be part of a directly displayable
 * section of a document.  This includes SVGGeometryFrame and SVGGFrame.
 * (Even though the latter doesn't display anything itself, if it contains
 * SVGGeometryFrame descendants it is can still be part of a displayable
 * section of a document)  This class is not used for elements that can never
 * display directly, including SVGGradientFrame and SVGPatternFrame.  (The
 * latter may contain displayable content, but it and its content are never
 * *directly* displayed in a document.  It can only end up being displayed by
 * means of a reference from other content.)
 *
 * Note specifically that SVG frames that inherit SVGContainerFrame do *not*
 * implement this class (only those that inherit SVGDisplayContainerFrame
 * do.)
 */
class ISVGDisplayableFrame : public nsQueryFrame {
 public:
  using imgDrawingParams = image::imgDrawingParams;

  NS_DECL_QUERYFRAME_TARGET(ISVGDisplayableFrame)

  /**
   * Paint this frame.
   *
   * SVG is painted using a combination of display lists (trees of
   * nsDisplayItem built by BuildDisplayList() implementations) and recursive
   * PaintSVG calls.  SVG frames with the NS_FRAME_IS_NONDISPLAY bit set are
   * always painted using recursive PaintSVG calls since display list painting
   * would provide no advantages (they wouldn't be retained for invalidation).
   * Displayed SVG is normally painted via a display list tree created under
   * SVGOuterSVGFrame::BuildDisplayList, In future we may use a PaintSVG() call
   * that recurses over the entire SVG frame tree on SVG container frames to
   * avoid display list construction when it is expensive and unnecessary (see
   * bug 934411).
   *
   * @param aTransform The transform that has to be multiplied onto the
   *   DrawTarget in order for drawing to be in this frame's SVG user space.
   *   Implementations of this method should avoid multiplying aTransform onto
   *   the DrawTarget when possible and instead just pass a transform down to
   *   their children.  This is preferable because changing the transform is
   *   very expensive for certain DrawTarget backends so it is best to minimize
   *   the number of transform changes.
   *
   * @param aImgParams imagelib parameters that may be used when painting
   *   feImage.
   */
  virtual void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams) = 0;

  /**
   * Returns the frame that should handle pointer events at aPoint.  aPoint is
   * expected to be in the SVG user space of the frame on which this method is
   * called.  The frame returned may be the frame on which this method is
   * called, any of its descendants or else nullptr.
   */
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) = 0;

  // Called on SVG child frames (except NS_FRAME_IS_NONDISPLAY frames)
  // to update and then invalidate their cached bounds. This method is not
  // called until after the SVGOuterSVGFrame has had its initial reflow
  // (i.e. once the SVG viewport dimensions are known). It should also only
  // be called by SVGOuterSVGFrame during its reflow.
  virtual void ReflowSVG() = 0;

  // Flags to pass to NotifySVGChange:
  //
  // TRANSFORM_CHANGED:
  //   the current transform matrix for this frame has changed
  // COORD_CONTEXT_CHANGED:
  //   the dimensions of this frame's coordinate context has changed (percentage
  //   lengths must be reevaluated)
  // FULL_ZOOM_CHANGED:
  //   the page's zoom level has changed
  enum SVGChangedFlags {
    TRANSFORM_CHANGED = 0x01,
    COORD_CONTEXT_CHANGED = 0x02,
    FULL_ZOOM_CHANGED = 0x04
  };
  /**
   * This is called on a frame when there has been a change to one of its
   * ancestors that might affect the frame too. SVGChangedFlags are passed
   * to indicate what changed.
   *
   * Implementations do not need to invalidate, since the caller will
   * invalidate the entire area of the ancestor that changed. However, they
   * may need to update their bounds.
   */
  virtual void NotifySVGChanged(uint32_t aFlags) = 0;

  /**
   * Get this frame's contribution to the rect returned by a GetBBox() call
   * that occurred either on this element, or on one of its ancestors.
   *
   * SVG defines an element's bbox to be the element's fill bounds in the
   * userspace established by that element. By allowing callers to pass in the
   * transform from the userspace established by this element to the userspace
   * established by an an ancestor, this method allows callers to obtain this
   * element's fill bounds in the userspace established by that ancestor
   * instead. In that case, since we return the bounds in a different userspace
   * (the ancestor's), the bounds we return are not this element's bbox, but
   * rather this element's contribution to the bbox of the ancestor.
   *
   * @param aToBBoxUserspace The transform from the userspace established by
   *   this element to the userspace established by the ancestor on which
   *   getBBox was called. This will be the identity matrix if we are the
   *   element on which getBBox was called.
   *
   * @param aFlags Flags indicating whether, stroke, for example, should be
   *   included in the bbox calculation.
   */
  virtual SVGBBox GetBBoxContribution(const gfx::Matrix& aToBBoxUserspace,
                                      uint32_t aFlags) = 0;

  // Are we a container frame?
  virtual bool IsDisplayContainer() = 0;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_ISVGDISPLAYABLEFRAME_H_
