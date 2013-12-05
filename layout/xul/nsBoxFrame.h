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

// flags from box
#define NS_STATE_BOX_CHILD_RESERVED      NS_FRAME_STATE_BIT(20)
#define NS_STATE_STACK_NOT_POSITIONED    NS_FRAME_STATE_BIT(21)
//#define NS_STATE_IS_HORIZONTAL           NS_FRAME_STATE_BIT(22)  moved to nsIFrame.h
#define NS_STATE_AUTO_STRETCH            NS_FRAME_STATE_BIT(23)
//#define NS_STATE_IS_ROOT                 NS_FRAME_STATE_BIT(24)  moved to nsBox.h
#define NS_STATE_CURRENTLY_IN_DEBUG      NS_FRAME_STATE_BIT(25)
//#define NS_STATE_SET_TO_DEBUG            NS_FRAME_STATE_BIT(26)  moved to nsBox.h
//#define NS_STATE_DEBUG_WAS_SET           NS_FRAME_STATE_BIT(27)  moved to nsBox.h
#define NS_STATE_MENU_HAS_POPUP_LIST       NS_FRAME_STATE_BIT(28) /* used on nsMenuFrame */
#define NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK NS_FRAME_STATE_BIT(29)
#define NS_STATE_EQUAL_SIZE              NS_FRAME_STATE_BIT(30)
//#define NS_STATE_IS_DIRECTION_NORMAL     NS_FRAME_STATE_BIT(31)  moved to nsIFrame.h
#define NS_FRAME_MOUSE_THROUGH_ALWAYS    NS_FRAME_STATE_BIT(60)
#define NS_FRAME_MOUSE_THROUGH_NEVER     NS_FRAME_STATE_BIT(61)

nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext,
                         bool aIsRoot,
                         nsBoxLayout* aLayoutManager);
nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext);

class nsBoxFrame : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell, 
                                  nsStyleContext* aContext,
                                  bool aIsRoot,
                                  nsBoxLayout* aLayoutManager);
  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

  // gets the rect inside our border and debug border. If you wish to paint inside a box
  // call this method to get the rect so you don't draw on the debug border or outer border.

  virtual void SetLayoutManager(nsBoxLayout* aLayout) MOZ_OVERRIDE { mLayoutManager = aLayout; }
  virtual nsBoxLayout* GetLayoutManager() MOZ_OVERRIDE { return mLayoutManager; }

  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIFrame* aChild) MOZ_OVERRIDE;

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aBoxLayoutState, bool aDebug) MOZ_OVERRIDE;
  NS_IMETHOD GetDebug(bool& aDebug) MOZ_OVERRIDE;
#endif
  virtual Valignment GetVAlign() const MOZ_OVERRIDE { return mValign; }
  virtual Halignment GetHAlign() const MOZ_OVERRIDE { return mHalign; }
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  virtual bool ComputesOwnOverflowArea() MOZ_OVERRIDE { return false; }

  // ----- child and sibling operations ---

  // ----- public methods -------
  
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        asPrevInFlow) MOZ_OVERRIDE;

 
  NS_IMETHOD AttributeChanged(int32_t         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              int32_t         aModType) MOZ_OVERRIDE;

  virtual void MarkIntrinsicWidthsDirty() MOZ_OVERRIDE;
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual nsIFrame* GetContentInsertionFrame() MOZ_OVERRIDE;

  NS_IMETHOD  SetInitialChildList(ChildListID     aListID,
                                  nsFrameList&    aChildList) MOZ_OVERRIDE;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
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

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus) MOZ_OVERRIDE;

  virtual bool HonorPrintBackgroundSettings() MOZ_OVERRIDE;

  virtual ~nsBoxFrame();
  
  nsBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot = false, nsBoxLayout* aLayoutManager = nullptr);

  // virtual so nsStackFrame, nsButtonBoxFrame, nsSliderFrame and nsMenuFrame
  // can override it
  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
  
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
    virtual void GetBoxName(nsAutoString& aName) MOZ_OVERRIDE;
    void PaintXULDebugBackground(nsRenderingContext& aRenderingContext,
                                 nsPoint aPt);
    void PaintXULDebugOverlay(nsRenderingContext& aRenderingContext,
                              nsPoint aPt);
#endif

    virtual bool GetInitialEqualSize(bool& aEqualSize); 
    virtual void GetInitialOrientation(bool& aIsHorizontal);
    virtual void GetInitialDirection(bool& aIsNormal);
    virtual bool GetInitialHAlignment(Halignment& aHalign); 
    virtual bool GetInitialVAlignment(Valignment& aValign); 
    virtual bool GetInitialAutoStretch(bool& aStretch); 
  
    virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

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
    bool GetEventPoint(mozilla::WidgetGUIEvent* aEvent, nsIntPoint& aPoint);

protected:
    void RegUnregAccessKey(bool aDoReg);

  NS_HIDDEN_(void) CheckBoxOrder();

private: 

#ifdef DEBUG_LAYOUT
    nsresult SetDebug(nsPresContext* aPresContext, bool aDebug);
    bool GetInitialDebug(bool& aDebug);
    void GetDebugPref(nsPresContext* aPresContext);

    void GetDebugBorder(nsMargin& aInset);
    void GetDebugPadding(nsMargin& aInset);
    void GetDebugMargin(nsMargin& aInset);

    nsresult GetFrameSizeWithMargin(nsIFrame* aBox, nsSize& aSize);

    void PixelMarginToTwips(nsPresContext* aPresContext, nsMargin& aMarginPixels);

    void GetValue(nsPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
    void GetValue(nsPresContext* aPresContext, int32_t a, int32_t b, char* value);
    void DrawSpacer(nsPresContext* aPresContext, nsRenderingContext& aRenderingContext, bool aHorizontal, int32_t flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize);
    void DrawLine(nsRenderingContext& aRenderingContext,  bool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsRenderingContext& aRenderingContext,  bool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);
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

