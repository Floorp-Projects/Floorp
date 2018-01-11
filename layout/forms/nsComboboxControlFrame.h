/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

class nsComboboxControlFrame final : public nsBlockFrame,
                                     public nsIFormControlFrame,
                                     public nsIComboboxControlFrame,
                                     public nsIAnonymousContentCreator,
                                     public nsISelectControlFrame,
                                     public nsIRollupListener,
                                     public nsIStatefulFrame
{
  typedef mozilla::gfx::DrawTarget DrawTarget;

public:
  friend nsComboboxControlFrame* NS_NewComboboxControlFrame(nsIPresShell* aPresShell,
                                                            nsStyleContext* aContext,
                                                            nsFrameState aFlags);
  friend class nsComboboxDisplayFrame;

  explicit nsComboboxControlFrame(nsStyleContext* aContext);
  ~nsComboboxControlFrame();

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsComboboxControlFrame)

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  nsIContent* GetDisplayNode() { return mDisplayContent; }
  nsIFrame* CreateFrameForDisplayNode();

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;

  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;

  virtual void Reflow(nsPresContext*           aCX,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;

  void PaintFocus(DrawTarget& aDrawTarget, nsPoint aPt);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual nsIScrollableFrame* GetScrollTargetFrame() override {
    return do_QueryFrame(mDropdownFrame);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual const nsFrameList& GetChildList(ChildListID aListID) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;

  virtual nsContainerFrame* GetContentInsertionFrame() override;

  // Return the dropdown and display frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) override;
  /**
   * Inform the control that it got (or lost) focus.
   * If it lost focus, the dropdown menu will be rolled up if needed,
   * and FireOnChange() will be called.
   * @param aOn true if got focus, false if lost focus.
   * @param aRepaint if true then force repaint (NOTE: we always force repaint currently)
   * @note This method might destroy |this|.
   */
  virtual void SetFocus(bool aOn, bool aRepaint) override;

  //nsIComboboxControlFrame
  virtual bool IsDroppedDown() override { return mDroppedDown; }
  /**
   * @note This method might destroy |this|.
   */
  virtual void ShowDropDown(bool aDoDropDown) override;
  virtual nsIFrame* GetDropDown() override;
  virtual void SetDropDown(nsIFrame* aDropDownFrame) override;
  /**
   * @note This method might destroy |this|.
   */
  virtual void RollupFromList() override;

  /**
   * Return the available space before and after this frame for
   * placing the drop-down list, and the current 2D translation.
   * Note that either or both can be less than or equal to zero,
   * if both are then the drop-down should be closed.
   */
  void GetAvailableDropdownSpace(mozilla::WritingMode aWM,
                                 nscoord* aBefore,
                                 nscoord* aAfter,
                                 mozilla::LogicalPoint* aTranslation);
  virtual int32_t GetIndexOfDisplayArea() override;
  /**
   * @note This method might destroy |this|.
   */
  NS_IMETHOD RedisplaySelectedText() override;
  virtual int32_t UpdateRecentIndex(int32_t aIndex) override;
  virtual void OnContentReset() override;


  bool IsOpenInParentProcess() override
  {
    return mIsOpenInParentProcess;
  }

  void SetOpenInParentProcess(bool aVal) override
  {
    mIsOpenInParentProcess = aVal;
  }

  // nsISelectControlFrame
  NS_IMETHOD AddOption(int32_t index) override;
  NS_IMETHOD RemoveOption(int32_t index) override;
  NS_IMETHOD DoneAddingChildren(bool aIsDone) override;
  NS_IMETHOD OnOptionSelected(int32_t aIndex, bool aSelected) override;
  NS_IMETHOD OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) override;

  //nsIRollupListener
  /**
   * Hide the dropdown menu and stop capturing mouse events.
   * @note This method might destroy |this|.
   */
  virtual bool Rollup(uint32_t aCount, bool aFlush,
                      const nsIntPoint* pos, nsIContent** aLastRolledUp) override;
  virtual void NotifyGeometryChange() override;

  /**
   * A combobox should roll up if a mousewheel event happens outside of
   * the popup area.
   */
  virtual bool ShouldRollupOnMouseWheelEvent() override
    { return true; }

  virtual bool ShouldConsumeOnMouseWheelEvent() override
    { return false; }

  /**
   * A combobox should not roll up if activated by a mouse activate message
   * (eg. X-mouse).
   */
  virtual bool ShouldRollupOnMouseActivate() override
    { return false; }

  virtual uint32_t GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain) override
    { return 0; }

  virtual nsIWidget* GetRollupWidget() override;

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) override;
  NS_IMETHOD RestoreState(nsPresState* aState) override;
  NS_IMETHOD GenerateStateKey(nsIContent* aContent,
                              nsIDocument* aDocument,
                              nsACString& aKey) override;

  static bool ToolkitHasNativePopup();

protected:
  friend class RedisplayTextEvent;
  friend class nsAsyncResize;
  friend class nsResizeDropdownAtFinalPosition;

  // Utilities
  void ReflowDropdown(nsPresContext*          aPresContext,
                      const ReflowInput& aReflowInput);

  // Return true if we should render a dropdown button.
  bool HasDropDownButton() const;

  enum DropDownPositionState {
    // can't show the dropdown at its current position
    eDropDownPositionSuppressed,
    // a resize reflow is pending, don't show it yet
    eDropDownPositionPendingResize,
    // the dropdown has its final size and position and can be displayed here
    eDropDownPositionFinal
  };
  DropDownPositionState AbsolutelyPositionDropDown();

  // Helper for GetMinISize/GetPrefISize
  nscoord GetIntrinsicISize(gfxContext* aRenderingContext,
                            nsLayoutUtils::IntrinsicISizeType aType);

  class RedisplayTextEvent : public mozilla::Runnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit RedisplayTextEvent(nsComboboxControlFrame* c)
      : mozilla::Runnable("nsComboboxControlFrame::RedisplayTextEvent")
      , mControlFrame(c)
    {
    }
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
  nsresult RedisplayText();
  void HandleRedisplayTextEvent();
  void ActuallyDisplayText(bool aNotify);

private:
  // If our total transform to the root frame of the root document is only a 2d
  // translation then return that translation, otherwise returns (0,0).
  nsPoint GetCSSTransformTranslation();

protected:
  nsFrameList              mPopupFrames;             // additional named child list
  nsCOMPtr<nsIContent>     mDisplayContent;          // Anonymous content used to display the current selection
  RefPtr<Element>     mButtonContent;                // Anonymous content for the button
  nsContainerFrame*        mDisplayFrame;            // frame to display selection
  nsIFrame*                mButtonFrame;             // button frame
  nsIFrame*                mDropdownFrame;           // dropdown list frame
  nsIListControlFrame *    mListControlFrame;        // ListControl Interface for the dropdown frame

  // The inline size of our display area.  Used by that frame's reflow
  // to size to the full inline size except the drop-marker.
  nscoord mDisplayISize;

  nsRevocableEventPtr<RedisplayTextEvent> mRedisplayTextEvent;

  int32_t               mRecentSelectedIndex;
  int32_t               mDisplayedIndex;
  nsString              mDisplayedOptionTextOrPreview;

  // make someone to listen to the button. If its programmatically pressed by someone like Accessibility
  // then open or close the combo box.
  nsCOMPtr<nsIDOMEventListener> mButtonListener;

  // The last y-positions used for estimating available space before and
  // after for the dropdown list in GetAvailableDropdownSpace.  These are
  // reset to nscoord_MIN in AbsolutelyPositionDropDown when placing the
  // dropdown at its actual position.  The GetAvailableDropdownSpace call
  // from nsListControlFrame::ReflowAsDropdown use the last position.
  nscoord               mLastDropDownBeforeScreenBCoord;
  nscoord               mLastDropDownAfterScreenBCoord;
  // Current state of the dropdown list, true is dropped down.
  bool                  mDroppedDown;
  // See comment in HandleRedisplayTextEvent().
  bool                  mInRedisplayText;
  // Acting on ShowDropDown(true) is delayed until we're focused.
  bool                  mDelayedShowDropDown;

  bool                  mIsOpenInParentProcess;

  // static class data member for Bug 32920
  // only one control can be focused at a time
  static nsComboboxControlFrame* sFocused;

#ifdef DO_REFLOW_COUNTER
  int32_t mReflowId;
#endif
};

#endif
