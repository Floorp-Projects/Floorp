/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  nsBoxFrame is a frame that can lay its children out either vertically or horizontally.
  It lays them out according to a min max or preferred size.
 
**/

#ifndef nsBoxFrame_h___
#define nsBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsBoxLayout.h"

class nsBoxLayoutState;

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext,
                         bool aIsRoot,
                         nsBoxLayout* aLayoutManager);
nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext);

class nsBoxFrame : public nsContainerFrame
{
protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;

public:
  NS_DECL_FRAMEARENA_HELPERS
#ifdef DEBUG
  NS_DECL_QUERYFRAME_TARGET(nsBoxFrame)
  NS_DECL_QUERYFRAME
#endif

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell, 
                                  nsStyleContext* aContext,
                                  bool aIsRoot,
                                  nsBoxLayout* aLayoutManager);
  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

  // gets the rect inside our border and debug border. If you wish to paint inside a box
  // call this method to get the rect so you don't draw on the debug border or outer border.

  virtual void SetLayoutManager(nsBoxLayout* aLayout) override { mLayoutManager = aLayout; }
  virtual nsBoxLayout* GetLayoutManager() override { return mLayoutManager; }

  virtual nsresult RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIFrame* aChild) override;

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;
#ifdef DEBUG_LAYOUT
  virtual nsresult SetDebug(nsBoxLayoutState& aBoxLayoutState, bool aDebug) override;
  virtual nsresult GetDebug(bool& aDebug) override;
#endif
  virtual Valignment GetVAlign() const override { return mValign; }
  virtual Halignment GetHAlign() const override { return mHalign; }
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) override;

  virtual bool ComputesOwnOverflowArea() override { return false; }

  // ----- child and sibling operations ---

  // ----- public methods -------
  
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

 
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

  virtual void MarkIntrinsicISizesDirty() override;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) override;

  virtual void SetInitialChildList(ChildListID  aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual nsIAtom* GetType() const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // record that children that are ignorable whitespace should be excluded 
    // (When content was loaded via the XUL content sink, it's already
    // been excluded, but we need this for when the XUL namespace is used
    // in other MIME types or when the XUL CSS display types are used with
    // non-XUL elements.)

    // This is bogus, but it's what we've always done.
    // (Given that we're replaced, we need to say we're a replaced element
    // that contains a block so nsHTMLReflowState doesn't tell us to be
    // NS_INTRINSICSIZE wide.)
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock | eXULBox |
        nsIFrame::eExcludesIgnorableWhitespace));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual void DidReflow(nsPresContext*           aPresContext,
                         const nsHTMLReflowState* aReflowState,
                         nsDidReflowStatus        aStatus) override;

  virtual bool HonorPrintBackgroundSettings() override;

  virtual ~nsBoxFrame();
  
  explicit nsBoxFrame(nsStyleContext* aContext, bool aIsRoot = false, nsBoxLayout* aLayoutManager = nullptr);

  // virtual so nsStackFrame, nsButtonBoxFrame, nsSliderFrame and nsMenuFrame
  // can override it
  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;
  
#ifdef DEBUG_LAYOUT
    virtual void SetDebugOnChildList(nsBoxLayoutState& aState, nsIFrame* aChild, bool aDebug);
    nsresult DisplayDebugInfoFor(nsIFrame*  aBox, 
                                 nsPoint& aPoint);
#endif

  static nsresult LayoutChildAt(nsBoxLayoutState& aState, nsIFrame* aBox, const nsRect& aRect);

  /**
   * Utility method to redirect events on descendants to this frame.
   * Supports 'allowevents' attribute on descendant elements to allow those
   * elements and their descendants to receive events.
   */
  void WrapListsInRedirector(nsDisplayListBuilder*   aBuilder,
                             const nsDisplayListSet& aIn,
                             const nsDisplayListSet& aOut);

  /**
   * This defaults to true, but some box frames (nsListBoxBodyFrame for
   * example) don't support ordinals in their children.
   */
  virtual bool SupportsOrdinalsInChildren();

protected:
#ifdef DEBUG_LAYOUT
    virtual void GetBoxName(nsAutoString& aName) override;
    void PaintXULDebugBackground(nsRenderingContext& aRenderingContext,
                                 nsPoint aPt);
    void PaintXULDebugOverlay(DrawTarget& aRenderingContext,
                              nsPoint aPt);
#endif

    virtual bool GetInitialEqualSize(bool& aEqualSize); 
    virtual void GetInitialOrientation(bool& aIsHorizontal);
    virtual void GetInitialDirection(bool& aIsNormal);
    virtual bool GetInitialHAlignment(Halignment& aHalign); 
    virtual bool GetInitialVAlignment(Valignment& aValign); 
    virtual bool GetInitialAutoStretch(bool& aStretch); 
  
    virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

    nsSize mPrefSize;
    nsSize mMinSize;
    nsSize mMaxSize;
    nscoord mFlex;
    nscoord mAscent;

    nsCOMPtr<nsBoxLayout> mLayoutManager;

    // Get the point associated with this event. Returns true if a single valid
    // point was found. Otherwise false.
    bool GetEventPoint(mozilla::WidgetGUIEvent* aEvent, nsPoint& aPoint);
    // Gets the event coordinates relative to the widget offset associated with
    // this frame. Return true if a single valid point was found.
    bool GetEventPoint(mozilla::WidgetGUIEvent* aEvent,
                       mozilla::LayoutDeviceIntPoint& aPoint);

protected:
    void RegUnregAccessKey(bool aDoReg);

  void CheckBoxOrder();

private: 

#ifdef DEBUG_LAYOUT
    nsresult SetDebug(nsPresContext* aPresContext, bool aDebug);
    bool GetInitialDebug(bool& aDebug);
    void GetDebugPref();

    void GetDebugBorder(nsMargin& aInset);
    void GetDebugPadding(nsMargin& aInset);
    void GetDebugMargin(nsMargin& aInset);

    nsresult GetFrameSizeWithMargin(nsIFrame* aBox, nsSize& aSize);

    void PixelMarginToTwips(nsMargin& aMarginPixels);

    void GetValue(nsPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
    void GetValue(nsPresContext* aPresContext, int32_t a, int32_t b, char* value);
    void DrawSpacer(nsPresContext* aPresContext, DrawTarget& aDrawTarget, bool aHorizontal, int32_t flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize);
    void DrawLine(DrawTarget& aDrawTarget,  bool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(DrawTarget& aDrawTarget,  bool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);
#endif
    virtual void UpdateMouseThrough();

    void CacheAttributes();

    // instance variables.
    Halignment mHalign;
    Valignment mValign;

#ifdef DEBUG_LAYOUT
    static bool gDebug;
    static nsIFrame* mDebugChild;
#endif

}; // class nsBoxFrame

#endif

