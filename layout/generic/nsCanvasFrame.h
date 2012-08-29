/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#ifndef nsCanvasFrame_h___
#define nsCanvasFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsGkAtoms.h"

class nsPresContext;
class nsRenderingContext;
class nsEvent;

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame
 */
class nsCanvasFrame : public nsContainerFrame,
                      public nsIScrollPositionListener
{
public:
  nsCanvasFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext),
    mDoPaintFocus(false),
    mAddedScrollPositionListener(false) {}

  NS_DECL_QUERYFRAME_TARGET(nsCanvasFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS


  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList) MOZ_OVERRIDE;
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;
  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers));
  }

  /** SetHasFocus tells the CanvasFrame to draw with focus ring
   *  @param aHasFocus true to show focus ring, false to hide it
   */
  NS_IMETHOD SetHasFocus(bool aHasFocus);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY);
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) {}

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              bool           aForceNormal) MOZ_OVERRIDE
  {
    NS_ASSERTION(!aForceNormal, "No-one should be passing this in here");

    // nsCanvasFrame keeps overflow container continuations of its child
    // frame in main child list
    nsresult rv = nsContainerFrame::StealFrame(aPresContext, aChild, true);
    if (NS_FAILED(rv)) {
      rv = nsContainerFrame::StealFrame(aPresContext, aChild);
    }
    return rv;
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  NS_IMETHOD GetContentForEvent(nsEvent* aEvent,
                                nsIContent** aContent) MOZ_OVERRIDE;

  nsRect CanvasArea() const;

protected:
  virtual int GetSkipSides() const;

  // Data members
  bool                      mDoPaintFocus;
  bool                      mAddedScrollPositionListener;
};

class nsDisplayCanvasBackgroundGeometry : public nsDisplayItemGeometry
{
public:
  nsDisplayCanvasBackgroundGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder, const nsRect& aChildBorder)
    : nsDisplayItemGeometry(aItem, aBuilder)
    , mChildBorder(aChildBorder)
    , mPaddingRect(aItem->GetPaddingRect())
    , mContentRect(aItem->GetContentRect())
  {}

  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
    mPaddingRect.MoveBy(aOffset);
    mContentRect.MoveBy(aOffset);
  }

  nsRect mChildBorder;
  nsRect mPaddingRect;
  nsRect mContentRect;
};

/**
 * Override nsDisplayBackground methods so that we pass aBGClipRect to
 * PaintBackground, covering the whole overflow area.
 * We can also paint an "extra background color" behind the normal
 * background.
 */
class nsDisplayCanvasBackground : public nsDisplayBackground {
public:
  nsDisplayCanvasBackground(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame, uint32_t aLayer)
    : nsDisplayBackground(aBuilder, aFrame, aLayer, true)
  {
    mExtraBackgroundColor = NS_RGBA(0,0,0,0);
  }

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion,
                                 const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE
  {
    return NS_GET_A(mExtraBackgroundColor) > 0 ||
      nsDisplayBackground::ComputeVisibility(aBuilder, aVisibleRegion,
                                             aAllowVisibleRegionExpansion);
  }
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE
  {
    if (NS_GET_A(mExtraBackgroundColor) == 255) {
      return nsRegion(GetBounds(aBuilder, aSnap));
    }
    return nsDisplayBackground::GetOpaqueRegion(aBuilder, aSnap);
  }
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE
  {
    nscolor background;
    if (!nsDisplayBackground::IsUniform(aBuilder, &background))
      return false;
    NS_ASSERTION(background == NS_RGBA(0,0,0,0),
                 "The nsDisplayBackground for a canvas frame doesn't paint "
                 "its background color normally");
    *aColor = mExtraBackgroundColor;
    return true;
  }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE
  {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    *aSnap = true;
    return frame->CanvasArea() + ToReferenceFrame();
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE
  {
    // We need to override so we don't consider border-radius.
    aOutFrames->AppendElement(mFrame);
  }
  
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder)
  {
    nsIFrame *child = mFrame->GetFirstPrincipalChild();
    return new nsDisplayCanvasBackgroundGeometry(this, aBuilder, 
                                                 child ? child->GetRect() : nsRect());;
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion)
  {
    const nsDisplayCanvasBackgroundGeometry* geometry = static_cast<const nsDisplayCanvasBackgroundGeometry*>(aGeometry);
    if (ShouldFixToViewport(aBuilder)) {
      // This is incorrect, We definitely need to check more things here. 
      return;
    }

    nsIFrame *child = mFrame->GetFirstPrincipalChild();

    bool snap;
    if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
        (child && !geometry->mChildBorder.IsEqualInterior(child->GetRect())) ||
        !geometry->mPaddingRect.IsEqualInterior(GetPaddingRect()) ||
        !geometry->mContentRect.IsEqualInterior(GetContentRect())) {
      if (!RenderingMightDependOnFrameSize() && geometry->mBounds.TopLeft() == GetBounds(aBuilder, &snap).TopLeft()) {
        aInvalidRegion->Xor(GetBounds(aBuilder, &snap), geometry->mBounds);
      } else {
        aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
      }
    }
  }
  
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;

  void SetExtraBackgroundColor(nscolor aColor)
  {
    mExtraBackgroundColor = aColor;
  }

  NS_DISPLAY_DECL_NAME("CanvasBackground", TYPE_CANVAS_BACKGROUND)

private:
  nscolor mExtraBackgroundColor;
};


#endif /* nsCanvasFrame_h___ */
