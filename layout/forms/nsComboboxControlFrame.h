/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComboboxControlFrame_h___
#define nsComboboxControlFrame_h___

#ifdef DEBUG_evaughan
//#define DEBUG_rods
#endif

#ifdef DEBUG_rods
//#define DO_REFLOW_DEBUG
//#define DO_REFLOW_COUNTER
//#define DO_UNCONSTRAINED_CHECK
//#define DO_PIXELS
//#define DO_NEW_REFLOW
#endif

//Mark used to indicate when onchange has been fired for current combobox item
#define NS_SKIP_NOTIFY_INDEX -2

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsISelectControlFrame.h"
#include "nsIRollupListener.h"
#include "nsIStatefulFrame.h"
#include "nsThreadUtils.h"

class nsStyleContext;
class nsIListControlFrame;
class nsComboboxDisplayFrame;
class nsIDOMEventListener;
class nsIScrollableFrame;

class nsComboboxControlFrame : public nsBlockFrame,
                               public nsIFormControlFrame,
                               public nsIComboboxControlFrame,
                               public nsIAnonymousContentCreator,
                               public nsISelectControlFrame,
                               public nsIRollupListener,
                               public nsIStatefulFrame
{
public:
  friend nsIFrame* NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, uint32_t aFlags);
  friend class nsComboboxDisplayFrame;

  nsComboboxControlFrame(nsStyleContext* aContext);
  ~nsComboboxControlFrame();

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent) MOZ_OVERRIDE;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt);

  // XXXbz this is only needed to prevent the quirk percent height stuff from
  // leaking out of the combobox.  We may be able to get rid of this as more
  // things move to IsFrameOfType.
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual nsIScrollableFrame* GetScrollTargetFrame() {
    return do_QueryFrame(mDropdownFrame);
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;
  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList) MOZ_OVERRIDE;
  virtual const nsFrameList& GetChildList(ChildListID aListID) const MOZ_OVERRIDE;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const MOZ_OVERRIDE;

  virtual nsIFrame* GetContentInsertionFrame();

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) MOZ_OVERRIDE;
  /**
   * Inform the control that it got (or lost) focus.
   * If it lost focus, the dropdown menu will be rolled up if needed,
   * and FireOnChange() will be called.
   * @param aOn true if got focus, false if lost focus.
   * @param aRepaint if true then force repaint (NOTE: we always force repaint currently)
   * @note This method might destroy |this|.
   */
  virtual void SetFocus(bool aOn, bool aRepaint) MOZ_OVERRIDE;

  //nsIComboboxControlFrame
  virtual bool IsDroppedDown() MOZ_OVERRIDE { return mDroppedDown; }
  /**
   * @note This method might destroy |this|.
   */
  virtual void ShowDropDown(bool aDoDropDown) MOZ_OVERRIDE;
  virtual nsIFrame* GetDropDown() MOZ_OVERRIDE;
  virtual void SetDropDown(nsIFrame* aDropDownFrame) MOZ_OVERRIDE;
  /**
   * @note This method might destroy |this|.
   */
  virtual void RollupFromList() MOZ_OVERRIDE;

  /**
   * Return the available space above and below this frame for
   * placing the drop-down list, and the current 2D translation.
   * Note that either or both can be less than or equal to zero,
   * if both are then the drop-down should be closed.
   */
  void GetAvailableDropdownSpace(nscoord* aAbove,
                                 nscoord* aBelow,
                                 nsPoint* aTranslation);
  virtual int32_t GetIndexOfDisplayArea() MOZ_OVERRIDE;
  /**
   * @note This method might destroy |this|.
   */
  NS_IMETHOD RedisplaySelectedText() MOZ_OVERRIDE;
  virtual int32_t UpdateRecentIndex(int32_t aIndex) MOZ_OVERRIDE;
  virtual void OnContentReset() MOZ_OVERRIDE;

  // nsISelectControlFrame
  NS_IMETHOD AddOption(int32_t index) MOZ_OVERRIDE;
  NS_IMETHOD RemoveOption(int32_t index) MOZ_OVERRIDE;
  NS_IMETHOD DoneAddingChildren(bool aIsDone) MOZ_OVERRIDE;
  NS_IMETHOD OnOptionSelected(int32_t aIndex, bool aSelected) MOZ_OVERRIDE;
  NS_IMETHOD OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) MOZ_OVERRIDE;

  //nsIRollupListener
  /**
   * Hide the dropdown menu and stop capturing mouse events.
   * @note This method might destroy |this|.
   */
  virtual bool Rollup(uint32_t aCount, const nsIntPoint* pos, nsIContent** aLastRolledUp) MOZ_OVERRIDE;
  virtual void NotifyGeometryChange() MOZ_OVERRIDE;

  /**
   * A combobox should roll up if a mousewheel event happens outside of
   * the popup area.
   */
  virtual bool ShouldRollupOnMouseWheelEvent() MOZ_OVERRIDE
    { return true; }

  virtual bool ShouldConsumeOnMouseWheelEvent() MOZ_OVERRIDE
    { return false; }

  /**
   * A combobox should not roll up if activated by a mouse activate message
   * (eg. X-mouse).
   */
  virtual bool ShouldRollupOnMouseActivate() MOZ_OVERRIDE
    { return false; }

  virtual uint32_t GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain) MOZ_OVERRIDE
    { return 0; }

  virtual nsIWidget* GetRollupWidget() MOZ_OVERRIDE;

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) MOZ_OVERRIDE;
  NS_IMETHOD RestoreState(nsPresState* aState) MOZ_OVERRIDE;

  static bool ToolkitHasNativePopup();

protected:
  friend class RedisplayTextEvent;
  friend class nsAsyncResize;
  friend class nsResizeDropdownAtFinalPosition;

  // Utilities
  nsresult ReflowDropdown(nsPresContext*          aPresContext, 
                          const nsHTMLReflowState& aReflowState);

  enum DropDownPositionState {
    // can't show the dropdown at its current position
    eDropDownPositionSuppressed,
    // a resize reflow is pending, don't show it yet
    eDropDownPositionPendingResize,
    // the dropdown has its final size and position and can be displayed here
    eDropDownPositionFinal
  };
  DropDownPositionState AbsolutelyPositionDropDown();

  // Helper for GetMinWidth/GetPrefWidth
  nscoord GetIntrinsicWidth(nsRenderingContext* aRenderingContext,
                            nsLayoutUtils::IntrinsicWidthType aType);

  class RedisplayTextEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    RedisplayTextEvent(nsComboboxControlFrame *c) : mControlFrame(c) {}
    void Revoke() { mControlFrame = nullptr; }
  private:
    nsComboboxControlFrame *mControlFrame;
  };
  
  /**
   * Show or hide the dropdown list.
   * @note This method might destroy |this|.
   */
  void ShowPopup(bool aShowPopup);

  /**
   * Show or hide the dropdown list.
   * @param aShowList true to show, false to hide the dropdown.
   * @note This method might destroy |this|.
   * @return false if this frame is destroyed, true if still alive.
   */
  bool ShowList(bool aShowList);
  void CheckFireOnChange();
  void FireValueChangeEvent();
  nsresult RedisplayText(int32_t aIndex);
  void HandleRedisplayTextEvent();
  void ActuallyDisplayText(bool aNotify);

private:
  // If our total transform to the root frame of the root document is only a 2d
  // translation then return that translation, otherwise returns (0,0).
  nsPoint GetCSSTransformTranslation();

protected:
  nsFrameList              mPopupFrames;             // additional named child list
  nsCOMPtr<nsIContent>     mDisplayContent;          // Anonymous content used to display the current selection
  nsCOMPtr<nsIContent>     mButtonContent;           // Anonymous content for the button
  nsIFrame*                mDisplayFrame;            // frame to display selection
  nsIFrame*                mButtonFrame;             // button frame
  nsIFrame*                mDropdownFrame;           // dropdown list frame
  nsIListControlFrame *    mListControlFrame;        // ListControl Interface for the dropdown frame

  // The width of our display area.  Used by that frame's reflow to
  // size to the full width except the drop-marker.
  nscoord mDisplayWidth;
  
  nsRevocableEventPtr<RedisplayTextEvent> mRedisplayTextEvent;

  int32_t               mRecentSelectedIndex;
  int32_t               mDisplayedIndex;
  nsString              mDisplayedOptionText;

  // make someone to listen to the button. If its programmatically pressed by someone like Accessibility
  // then open or close the combo box.
  nsCOMPtr<nsIDOMEventListener> mButtonListener;

  // The last y-positions used for estimating available space above and
  // below for the dropdown list in GetAvailableDropdownSpace.  These are
  // reset to nscoord_MIN in AbsolutelyPositionDropDown when placing the
  // dropdown at its actual position.  The GetAvailableDropdownSpace call
  // from nsListControlFrame::ReflowAsDropdown use the last position.
  nscoord               mLastDropDownAboveScreenY;
  nscoord               mLastDropDownBelowScreenY;
  // Current state of the dropdown list, true is dropped down.
  bool                  mDroppedDown;
  // See comment in HandleRedisplayTextEvent().
  bool                  mInRedisplayText;
  // Acting on ShowDropDown(true) is delayed until we're focused.
  bool                  mDelayedShowDropDown;

  // static class data member for Bug 32920
  // only one control can be focused at a time
  static nsComboboxControlFrame* sFocused;

#ifdef DO_REFLOW_COUNTER
  int32_t mReflowId;
#endif
};

#endif
