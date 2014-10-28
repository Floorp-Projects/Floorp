/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#ifndef nsCanvasFrame_h___
#define nsCanvasFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsContainerFrame.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsIAnonymousContentCreator.h"

class nsPresContext;
class nsRenderingContext;

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame
 */
class nsCanvasFrame MOZ_FINAL : public nsContainerFrame,
                                public nsIScrollPositionListener,
                                public nsIAnonymousContentCreator
{
public:
  explicit nsCanvasFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext),
    mDoPaintFocus(false),
    mAddedScrollPositionListener(false) {}

  NS_DECL_QUERYFRAME_TARGET(nsCanvasFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS


  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) MOZ_OVERRIDE;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) MOZ_OVERRIDE;
#endif

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements, uint32_t aFilter) MOZ_OVERRIDE;

  // Touch caret handle function
  mozilla::dom::Element* GetTouchCaretElement() const
  {
     return mTouchCaretElement;
  }

  // Selection Caret Handle function
  mozilla::dom::Element* GetSelectionCaretsStartElement() const
  {
    return mSelectionCaretsStartElement;
  }

  mozilla::dom::Element* GetSelectionCaretsEndElement() const
  {
    return mSelectionCaretsEndElement;
  }

  mozilla::dom::Element* GetCustomContentContainer() const
  {
    return mCustomContentContainer;
  }

  /** SetHasFocus tells the CanvasFrame to draw with focus ring
   *  @param aHasFocus true to show focus ring, false to hide it
   */
  NS_IMETHOD SetHasFocus(bool aHasFocus);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) MOZ_OVERRIDE;
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) MOZ_OVERRIDE {}

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual nsresult StealFrame(nsIFrame* aChild, bool aForceNormal) MOZ_OVERRIDE
  {
    NS_ASSERTION(!aForceNormal, "No-one should be passing this in here");

    // nsCanvasFrame keeps overflow container continuations of its child
    // frame in main child list
    nsresult rv = nsContainerFrame::StealFrame(aChild, true);
    if (NS_FAILED(rv)) {
      rv = nsContainerFrame::StealFrame(aChild);
    }
    return rv;
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsresult GetContentForEvent(mozilla::WidgetEvent* aEvent,
                                      nsIContent** aContent) MOZ_OVERRIDE;

  nsRect CanvasArea() const;

protected:
  // Data members
  bool                      mDoPaintFocus;
  bool                      mAddedScrollPositionListener;

  nsCOMPtr<mozilla::dom::Element> mTouchCaretElement;
  nsCOMPtr<mozilla::dom::Element> mSelectionCaretsStartElement;
  nsCOMPtr<mozilla::dom::Element> mSelectionCaretsEndElement;
  nsCOMPtr<mozilla::dom::Element> mCustomContentContainer;
};

/**
 * Override nsDisplayBackground methods so that we pass aBGClipRect to
 * PaintBackground, covering the whole overflow area.
 * We can also paint an "extra background color" behind the normal
 * background.
 */
class nsDisplayCanvasBackgroundColor : public nsDisplayItem {
public:
  nsDisplayCanvasBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mColor(NS_RGBA(0,0,0,0))
  {
  }

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) MOZ_OVERRIDE
  {
    return NS_GET_A(mColor) > 0;
  }
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE
  {
    if (NS_GET_A(mColor) == 255) {
      return nsRegion(GetBounds(aBuilder, aSnap));
    }
    return nsRegion();
  }
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE
  {
    *aColor = mColor;
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

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE
  {
    return new nsDisplayItemBoundsGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) MOZ_OVERRIDE
  {
    const nsDisplayItemBoundsGeometry* geometry = static_cast<const nsDisplayItemBoundsGeometry*>(aGeometry);
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;

  void SetExtraBackgroundColor(nscolor aColor)
  {
    mColor = aColor;
  }

  NS_DISPLAY_DECL_NAME("CanvasBackgroundColor", TYPE_CANVAS_BACKGROUND_COLOR)
#ifdef MOZ_DUMP_PAINTING
  virtual void WriteDebugInfo(nsACString& aTo) MOZ_OVERRIDE;
#endif

private:
  nscolor mColor;
};

class nsDisplayCanvasBackgroundImage : public nsDisplayBackgroundImage {
public:
  nsDisplayCanvasBackgroundImage(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 uint32_t aLayer, const nsStyleBackground* aBg)
    : nsDisplayBackgroundImage(aBuilder, aFrame, aLayer, aBg)
  {}

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;

  virtual void NotifyRenderingChanged() MOZ_OVERRIDE
  {
    mFrame->Properties().Delete(nsIFrame::CachedBackgroundImage());
    mFrame->Properties().Delete(nsIFrame::CachedBackgroundImageDT());
  }

  virtual bool ShouldFixToViewport(LayerManager* aManager) MOZ_OVERRIDE
  {
    // Put background-attachment:fixed canvas background images in their own
    // compositing layer. Since we know their background painting area can't
    // change (unless the viewport size itself changes), async scrolling
    // will work well.
    return mBackgroundStyle->mLayers[mLayer].mAttachment == NS_STYLE_BG_ATTACHMENT_FIXED &&
           !mBackgroundStyle->mLayers[mLayer].mImage.IsEmpty();
  }
 
  // We still need to paint a background color as well as an image for this item, 
  // so we can't support this yet.
  virtual bool SupportsOptimizingToImage() MOZ_OVERRIDE { return false; }
  
  
  NS_DISPLAY_DECL_NAME("CanvasBackgroundImage", TYPE_CANVAS_BACKGROUND_IMAGE)
};

class nsDisplayCanvasThemedBackground : public nsDisplayThemedBackground {
public:
  nsDisplayCanvasThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayThemedBackground(aBuilder, aFrame)
  {}

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("CanvasThemedBackground", TYPE_CANVAS_THEMED_BACKGROUND)
};

#endif /* nsCanvasFrame_h___ */
