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
#include "nsDisplayList.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIPopupContainer.h"
#include "nsIScrollPositionListener.h"

class nsPresContext;
class gfxContext;

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
        mAddedScrollPositionListener(false) {}

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsCanvasFrame)

  Element* GetDefaultTooltip() override;

  void Destroy(DestroyContext&) override;

  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
#ifdef DEBUG
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;
#endif

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
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

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  void PaintFocus(mozilla::gfx::DrawTarget* aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  void ScrollPositionWillChange(nscoord aX, nscoord aY) override;
  void ScrollPositionDidChange(nscoord aX, nscoord aY) override {}

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif
  nsresult GetContentForEvent(const mozilla::WidgetEvent* aEvent,
                              nsIContent** aContent) override;

  nsRect CanvasArea() const;

 protected:
  // Data members
  bool mDoPaintFocus;
  bool mAddedScrollPositionListener;

  nsCOMPtr<Element> mCustomContentContainer;
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

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    *aSnap = true;
    return frame->CanvasArea() + ToReferenceFrame();
  }
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override {
    // We need to override so we don't consider border-radius.
    aOutFrames->AppendElement(mFrame);
  }
  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  void SetExtraBackgroundColor(nscolor aColor) { mColor = aColor; }

  NS_DISPLAY_DECL_NAME("CanvasBackgroundColor", TYPE_CANVAS_BACKGROUND_COLOR)

  void WriteDebugInfo(std::stringstream& aStream) override;
};

class nsDisplayCanvasBackgroundImage : public nsDisplayBackgroundImage {
 public:
  explicit nsDisplayCanvasBackgroundImage(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aFrame,
                                          const InitData& aInitData)
      : nsDisplayBackgroundImage(aBuilder, aFrame, aInitData) {}

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  // We still need to paint a background color as well as an image for this
  // item, so we can't support this yet.
  bool SupportsOptimizingToImage() const override { return false; }

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

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("CanvasThemedBackground", TYPE_CANVAS_THEMED_BACKGROUND)
};

}  // namespace mozilla

#endif /* nsCanvasFrame_h___ */
