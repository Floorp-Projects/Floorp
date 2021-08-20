/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIPopupContainer.h"
#include "nsDisplayList.h"
#include "nsIAnonymousContentCreator.h"

class nsPresContext;
class gfxContext;
class nsPopupSetFrame;

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame.
 * @note nsCanvasFrame keeps overflow container continuations of its child
 * frame in the main child list.
 */
class nsCanvasFrame final : public nsContainerFrame,
                            public nsIScrollPositionListener,
                            public nsIAnonymousContentCreator,
                            public nsIPopupContainer {
  using Element = mozilla::dom::Element;

 public:
  explicit nsCanvasFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID),
        mDoPaintFocus(false),
        mAddedScrollPositionListener(false),
        mPopupSetFrame(nullptr) {}

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsCanvasFrame)

  nsPopupSetFrame* GetPopupSetFrame() override;
  void SetPopupSetFrame(nsPopupSetFrame* aPopupSet) override;
  Element* GetDefaultTooltip() override;
  void SetDefaultTooltip(Element* aTooltip) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
#ifdef DEBUG
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
#endif

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eCanContainOverflowContainers));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  Element* GetCustomContentContainer() const { return mCustomContentContainer; }

  /**
   * Unhide the CustomContentContainer. This call only has an effect if
   * mCustomContentContainer is non-null.
   */
  void ShowCustomContentContainer();

  /**
   * Hide the CustomContentContainer. This call only has an effect if
   * mCustomContentContainer is non-null.
   */
  void HideCustomContentContainer();

  /** SetHasFocus tells the CanvasFrame to draw with focus ring
   *  @param aHasFocus true to show focus ring, false to hide it
   */
  NS_IMETHOD SetHasFocus(bool aHasFocus);

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  void PaintFocus(mozilla::gfx::DrawTarget* aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) override;
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) override {}

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
  virtual nsresult GetContentForEvent(mozilla::WidgetEvent* aEvent,
                                      nsIContent** aContent) override;

  nsRect CanvasArea() const;

 protected:
  // Data members
  bool mDoPaintFocus;
  bool mAddedScrollPositionListener;

  nsCOMPtr<Element> mCustomContentContainer;

 private:
  nsPopupSetFrame* mPopupSetFrame;
  nsCOMPtr<Element> mPopupgroupContent;
  nsCOMPtr<Element> mTooltipContent;
};

namespace mozilla {
/**
 * Override nsDisplayBackground methods so that we pass aBGClipRect to
 * PaintBackground, covering the whole overflow area.
 * We can also paint an "extra background color" behind the normal
 * background.
 */
class nsDisplayCanvasBackgroundColor final : public nsDisplaySolidColorBase {
 public:
  nsDisplayCanvasBackgroundColor(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame)
      : nsDisplaySolidColorBase(aBuilder, aFrame, NS_RGBA(0, 0, 0, 0)) {}

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override {
    return NS_GET_A(mColor) > 0;
  }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    *aSnap = true;
    return frame->CanvasArea() + ToReferenceFrame();
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override {
    // We need to override so we don't consider border-radius.
    aOutFrames->AppendElement(mFrame);
  }
  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  void SetExtraBackgroundColor(nscolor aColor) { mColor = aColor; }

  NS_DISPLAY_DECL_NAME("CanvasBackgroundColor", TYPE_CANVAS_BACKGROUND_COLOR)

  virtual void WriteDebugInfo(std::stringstream& aStream) override;
};

class nsDisplayCanvasBackgroundImage : public nsDisplayBackgroundImage {
 public:
  explicit nsDisplayCanvasBackgroundImage(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aFrame,
                                          const InitData& aInitData)
      : nsDisplayBackgroundImage(aBuilder, aFrame, aInitData) {}

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  // We still need to paint a background color as well as an image for this
  // item, so we can't support this yet.
  virtual bool SupportsOptimizingToImage() const override { return false; }

  bool IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aClipRect, gfxRect* aDestRect);

  NS_DISPLAY_DECL_NAME("CanvasBackgroundImage", TYPE_CANVAS_BACKGROUND_IMAGE)
};

class nsDisplayCanvasThemedBackground : public nsDisplayThemedBackground {
 public:
  nsDisplayCanvasThemedBackground(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame)
      : nsDisplayThemedBackground(aBuilder, aFrame,
                                  aFrame->GetRectRelativeToSelf() +
                                      aBuilder->ToReferenceFrame(aFrame)) {
    nsDisplayThemedBackground::Init(aBuilder);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("CanvasThemedBackground", TYPE_CANVAS_THEMED_BACKGROUND)
};

}  // namespace mozilla

#endif /* nsCanvasFrame_h___ */
