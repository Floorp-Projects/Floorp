/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_ISVGCHILDFRAME_H__
#define __NS_ISVGCHILDFRAME_H__

#include "gfxRect.h"
#include "nsQueryFrame.h"

class nsIFrame;
class nsRenderingContext;

struct nsPoint;
class SVGBBox;
struct nsRect;
struct nsIntRect;

namespace mozilla {
class SVGAnimatedLengthList;
class SVGAnimatedNumberList;
class SVGLengthList;
class SVGNumberList;
class SVGUserUnitList;

namespace gfx {
class Matrix;
}
}

/**
 * This class is not particularly well named. It is inherited by some, but
 * not all SVG frame classes that can be descendants of an
 * nsSVGOuterSVGFrame in the frame tree. Note specifically that SVG container
 * frames that do not inherit nsSVGDisplayContainerFrame do not inherit this
 * class (so that's classes that only inherit nsSVGContainerFrame).
 */
class nsISVGChildFrame : public nsQueryFrame
{
public:
  typedef mozilla::SVGAnimatedNumberList SVGAnimatedNumberList;
  typedef mozilla::SVGNumberList SVGNumberList;
  typedef mozilla::SVGAnimatedLengthList SVGAnimatedLengthList;
  typedef mozilla::SVGLengthList SVGLengthList;
  typedef mozilla::SVGUserUnitList SVGUserUnitList;

  NS_DECL_QUERYFRAME_TARGET(nsISVGChildFrame)

  // Paint this frame.
  // aDirtyRect is the area being redrawn, in frame offset pixel coordinates.
  // aTransformRoot (if non-null) is the frame at which we stop looking up
  // transforms, when painting content that is part of an SVG glyph. (See
  // bug 875329.)
  // For normal SVG graphics using display-list rendering, any transforms on
  // the element or its parents will have already been set up in the context
  // before PaintSVG is called. When painting SVG glyphs, this is not the case,
  // so the element's full transform needs to be applied; but we don't want to
  // apply transforms from outside the actual glyph element, so we need to know
  // how far up the ancestor chain to go.
  virtual nsresult PaintSVG(nsRenderingContext* aContext,
                            const nsIntRect *aDirtyRect,
                            nsIFrame* aTransformRoot = nullptr) = 0;

  // Check if this frame or children contain the given point,
  // specified in app units relative to the origin of the outer
  // svg frame (origin ill-defined in the case of borders - bug
  // 290770).  See bug 290852 for foreignObject complications.
  virtual nsIFrame* GetFrameForPoint(const nsPoint &aPoint)=0;

  // Get bounds in our nsSVGOuterSVGFrame's coordinates space (in app units)
  virtual nsRect GetCoveredRegion()=0;

  // Called on SVG child frames (except NS_FRAME_IS_NONDISPLAY frames)
  // to update and then invalidate their cached bounds. This method is not
  // called until after the nsSVGOuterSVGFrame has had its initial reflow
  // (i.e. once the SVG viewport dimensions are known). It should also only
  // be called by nsSVGOuterSVGFrame during its reflow.
  virtual void ReflowSVG()=0;

  /**
   * Flags used to specify to GetCanvasTM what it's being called for so that it
   * knows how far up the tree the "canvas" is. When display lists are being
   * used for painting or hit-testing of SVG, the "canvas" is simply user
   * space.
   */
  enum RequestingCanvasTMFor {
    FOR_PAINTING = 1,
    FOR_HIT_TESTING,
    FOR_OUTERSVG_TM
  };

  // Flags to pass to NotifySVGChange:
  //
  // DO_NOT_NOTIFY_RENDERING_OBSERVERS - this should only be used when
  //                           updating the descendant frames of a clipPath,
  //                           mask, pattern or marker frame (or other similar
  //                           NS_FRAME_IS_NONDISPLAY frame) immediately
  //                           prior to painting that frame's descendants.
  // TRANSFORM_CHANGED     - the current transform matrix for this frame has changed
  // COORD_CONTEXT_CHANGED - the dimensions of this frame's coordinate context has
  //                           changed (percentage lengths must be reevaluated)
  enum SVGChangedFlags {
    TRANSFORM_CHANGED     = 0x01,
    COORD_CONTEXT_CHANGED = 0x02,
    FULL_ZOOM_CHANGED     = 0x04
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
  virtual void NotifySVGChanged(uint32_t aFlags)=0;

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
  virtual SVGBBox GetBBoxContribution(const mozilla::gfx::Matrix &aToBBoxUserspace,
                                      uint32_t aFlags) = 0;

  // Are we a container frame?
  virtual bool IsDisplayContainer()=0;
};

#endif // __NS_ISVGCHILDFRAME_H__

