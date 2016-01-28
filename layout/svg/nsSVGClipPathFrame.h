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

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;

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
   * Applies the clipPath by pushing a clip path onto the DrawTarget.
   *
   * This method must only be used if IsTrivial() returns true, otherwise use
   * GetClipMask.
   *
   * @param aContext The context that the clip path is to be applied to.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to aContext's
   *   current transform.
   */
  void ApplyClipPath(gfxContext& aContext,
                     nsIFrame* aClippedFrame,
                     const gfxMatrix &aMatrix);

  /**
   * Returns an alpha mask surface containing the clipping geometry.
   *
   * This method must only be used if IsTrivial() returns false, otherwise use
   * ApplyClipPath.
   *
   * @param aReferenceContext Used to determine the backend for and size of the
   *   returned SourceSurface, the size being limited to the device space clip
   *   extents on the context.
   * @param aClippedFrame The/an nsIFrame of the element that references this
   *   clipPath that is currently being processed.
   * @param aMatrix The transform from aClippedFrame's user space to aContext's
   *   current transform.
   * @param [out] aMaskTransform The transform to use with the returned
   *   surface.
   * @param [in, optional] aExtraMask An extra surface that the returned
   *   surface should be masked with.
   * @param [in, optional] aExtraMasksTransform The transform to use with
   *   aExtraMask. Should be passed when aExtraMask is passed.
   */
  already_AddRefed<SourceSurface>
    GetClipMask(gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
                const gfxMatrix& aMatrix, Matrix* aMaskTransform,
                SourceSurface* aExtraMask = nullptr,
                const Matrix& aExtraMasksTransform = Matrix());

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

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  /**
   * SVG content may contain reference loops where an SVG effect (a clipPath,
   * say) may reference itself (directly or indirectly via a reference chain).
   * This helper class allows us to detect and break such reference loops when
   * applying an effect so that we can safely do so without the reference loop
   * causing us to recurse until we run out of stack space and crash.
   * The helper automatically sets and clears the mInUse flag on the frame.
   */
  class MOZ_RAII AutoReferenceLoopDetector
  {
  public:
    explicit AutoReferenceLoopDetector()
       : mFrame(nullptr)
#ifdef DEBUG
       , mMarkAsInUseCalled(false)
#endif
    {}

    ~AutoReferenceLoopDetector() {
      MOZ_ASSERT(mMarkAsInUseCalled,
                 "Instances of this class are useless if MarkAsInUse() is "
                 "not called on them");
      if (mFrame) {
        mFrame->mInUse = false;
      }
    }

    /**
     * Returns true on success (no reference loop), else returns false on
     * failure (aFrame is already in use; that is, there is a reference loop).
     */
    MOZ_WARN_UNUSED_RESULT bool MarkAsInUse(nsSVGClipPathFrame* aFrame) {
#ifdef DEBUG
      MOZ_ASSERT(!mMarkAsInUseCalled, "Must only be called once");
      mMarkAsInUseCalled = true;
#endif
      if (aFrame->mInUse) {
        // XXX This is an error in the document, not in Mozilla code, so stop
        // using NS_WARNING and send a message to the console instead.
        NS_WARNING("clipPath reference loop!");
        return false;
      }
      aFrame->mInUse = true;
      mFrame = aFrame;
      return true;
    }

  private:
    nsSVGClipPathFrame* mFrame;
    DebugOnly<bool> mMarkAsInUseCalled;
  };

  // Set, during a GetClipMask() call, to the transform that still needs to be
  // concatenated to the transform of the DrawTarget that was passed to
  // GetClipMask in order to establish the coordinate space that the clipPath
  // establishes for its contents (i.e. including applying 'clipPathUnits' and
  // any 'transform' attribute set on the clipPath) specifically for clipping
  // the frame that was passed to GetClipMask at that moment in time.  This is
  // set so that if our GetCanvasTM method is called while GetClipMask is
  // painting its children, the returned matrix will include the transforms
  // that should be used when creating the mask for the frame passed to
  // GetClipMask.
  //
  // Note: The removal of GetCanvasTM is nearly complete, so our GetCanvasTM
  // may not even be called soon/any more.
  gfxMatrix mMatrixForChildren;

  // Flag used by AutoReferenceLoopDetector to protect against reference loops:
  bool mInUse;
};

#endif
