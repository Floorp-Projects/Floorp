/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsListControlFrame_h___
#define nsListControlFrame_h___

#ifdef DEBUG_evaughan
//#define DEBUG_rods
#endif

#ifdef DEBUG_rods
//#define DO_REFLOW_DEBUG
//#define DO_REFLOW_COUNTER
//#define DO_UNCONSTRAINED_CHECK
//#define DO_PIXELS
#endif

#include "mozilla/Attributes.h"
#include "nsGfxScrollFrame.h"
#include "nsIFormControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsSelectsAreaFrame.h"

// X.h defines KeyPress
#ifdef KeyPress
#  undef KeyPress
#endif

class nsComboboxControlFrame;
class nsPresContext;
class nsListEventListener;

namespace mozilla {
class PresShell;
namespace dom {
class Event;
class HTMLOptionElement;
class HTMLSelectElement;
class HTMLOptionsCollection;
}  // namespace dom
}  // namespace mozilla

/**
 * Frame-based listbox.
 */

class nsListControlFrame final : public nsHTMLScrollFrame,
                                 public nsIFormControlFrame,
                                 public nsISelectControlFrame {
 public:
  typedef mozilla::dom::HTMLOptionElement HTMLOptionElement;

  friend nsContainerFrame* NS_NewListControlFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsListControlFrame)

  // nsIFrame
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;

  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;

  virtual void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void DidReflow(nsPresContext* aPresContext,
                         const ReflowInput* aReflowInput) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsHTMLScrollFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsAtom* aName,
                                   const nsAString& aValue) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void SetFocus(bool aOn = true, bool aRepaint = false) override;

  virtual mozilla::ScrollStyles GetScrollStyles() const override;
  virtual bool ShouldPropagateComputedBSizeToScrolledContent() const override;

  // for accessibility purposes
#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  void SetComboboxFrame(nsIFrame* aComboboxFrame);
  int32_t GetSelectedIndex();
  HTMLOptionElement* GetCurrentOption();

  /**
   * Gets the text of the currently selected item.
   * If the there are zero items then an empty string is returned
   * If there is nothing selected, then the 0th item's text is returned.
   */
  void GetOptionText(uint32_t aIndex, nsAString& aStr);

  void CaptureMouseEvents(bool aGrabMouseEvents);
  nscoord GetBSizeOfARow();
  uint32_t GetNumberOfOptions();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void AboutToDropDown();

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void AboutToRollup();

  /**
   * Dispatch a DOM oninput and onchange event synchroniously.
   * @note This method might destroy the frame, pres shell and other objects.
   */
  MOZ_CAN_RUN_SCRIPT
  void FireOnInputAndOnChange();

  /**
   * Makes aIndex the selected option of a combobox list.
   * @note This method might destroy the frame, pres shell and other objects.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ComboboxFinish(int32_t aIndex);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void OnContentReset();

  // nsISelectControlFrame
  NS_IMETHOD AddOption(int32_t index) override;
  NS_IMETHOD RemoveOption(int32_t index) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD DoneAddingChildren(bool aIsDone) override;

  /**
   * Gets the content (an option) by index and then set it as
   * being selected or not selected.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD OnOptionSelected(int32_t aIndex, bool aSelected) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD_(void)
  OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) override;

  /**
   * Mouse event listeners.
   * @note These methods might destroy the frame, pres shell and other objects.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult MouseDown(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult MouseUp(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult MouseMove(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult DragMove(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult KeyDown(mozilla::dom::Event* aKeyEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult KeyPress(mozilla::dom::Event* aKeyEvent);

  /**
   * Returns the options collection for mContent, if any.
   */
  mozilla::dom::HTMLOptionsCollection* GetOptions() const;
  /**
   * Returns the HTMLOptionElement for a given index in mContent's collection.
   */
  HTMLOptionElement* GetOption(uint32_t aIndex) const;

  static void ComboboxFocusSet();

  // Helper
  bool IsFocused() { return this == mFocused; }

  /**
   * Function to paint the focus rect when our nsSelectsAreaFrame is painting.
   * @param aPt the offset of this frame, relative to the rendering reference
   * frame
   */
  void PaintFocus(mozilla::gfx::DrawTarget* aDrawTarget, nsPoint aPt);

  /**
   * If this frame IsFocused(), invalidates an area that includes anything
   * that PaintFocus will or could have painted --- basically the whole
   * GetOptionsContainer, plus some extra stuff if there are no options. This
   * must be called every time mEndSelectionIndex changes.
   */
  void InvalidateFocus();

  /**
   * Function to calculate the block size of a row, for use with the
   * "size" attribute.
   * Can't be const because GetNumberOfOptions() isn't const.
   */
  nscoord CalcBSizeOfARow();

  /**
   * Function to ask whether we're currently in what might be the
   * first pass of a two-pass reflow.
   */
  bool MightNeedSecondPass() const { return mMightNeedSecondPass; }

  void SetSuppressScrollbarUpdate(bool aSuppress) {
    nsHTMLScrollFrame::SetSuppressScrollbarUpdate(aSuppress);
  }

  /**
   * Return whether the list is in dropdown mode.
   */
  bool IsInDropDownMode() const;

  /**
   * Return the number of displayed rows in the list.
   */
  uint32_t GetNumDisplayRows() const { return mNumDisplayRows; }

  /**
   * Return true if the drop-down list can display more rows.
   * (always false if not in drop-down mode)
   */
  bool GetDropdownCanGrow() const { return mDropdownCanGrow; }

  /**
   * Frees statics owned by this class.
   */
  static void Shutdown();

#ifdef ACCESSIBILITY
  /**
   * Post a custom DOM event for the change, so that accessibility can
   * fire a native focus event for accessibility
   * (Some 3rd party products need to track our focus)
   */
  void FireMenuItemActiveEvent();  // Inform assistive tech what got focused
#endif

 protected:
  /**
   * Return the first non-disabled option starting at aFromIndex (inclusive).
   * @param aFoundIndex if non-null, set to the index of the returned option
   */
  HTMLOptionElement* GetNonDisabledOptionFrom(int32_t aFromIndex,
                                              int32_t* aFoundIndex = nullptr);

  /**
   * Updates the selected text in a combobox and then calls FireOnChange().
   * @note This method might destroy the frame, pres shell and other objects.
   * Returns false if calling it destroyed |this|.
   */
  MOZ_CAN_RUN_SCRIPT
  bool UpdateSelection();

  /**
   * Returns whether mContent supports multiple selection.
   */
  bool GetMultiple() const {
    return mContent->AsElement()->HasAttr(kNameSpaceID_None,
                                          nsGkAtoms::multiple);
  }

  /**
   * Toggles (show/hide) the combobox dropdown menu.
   * @note This method might destroy the frame, pres shell and other objects.
   */
  MOZ_CAN_RUN_SCRIPT
  void DropDownToggleKey(mozilla::dom::Event* aKeyEvent);

  /**
   * @return true if the <option> at aIndex is selectable by the user.
   */
  bool IsOptionInteractivelySelectable(int32_t aIndex) const;
  /**
   * @return true if aOption in aSelect is selectable by the user.
   */
  static bool IsOptionInteractivelySelectable(
      mozilla::dom::HTMLSelectElement* aSelect,
      mozilla::dom::HTMLOptionElement* aOption);

  MOZ_CAN_RUN_SCRIPT void ScrollToFrame(HTMLOptionElement& aOptElement);

  MOZ_CAN_RUN_SCRIPT void ScrollToIndex(int32_t anIndex);

  /**
   * When the user clicks on the comboboxframe to show the dropdown
   * listbox, they then have to move the mouse into the list. We don't
   * want to process those mouse events as selection events (i.e., to
   * scroll list items into view). So we ignore the events until
   * the mouse moves below our border-inner-edge, when
   * mItemSelectionStarted is set.
   *
   * @param aPoint relative to this frame
   */
  bool IgnoreMouseEventForSelection(mozilla::dom::Event* aEvent);

  /**
   * If the dropdown is showing and the mouse has moved below our
   * border-inner-edge, then set mItemSelectionStarted.
   */
  void UpdateInListState(mozilla::dom::Event* aEvent);
  void AdjustIndexForDisabledOpt(int32_t aStartIndex, int32_t& anNewIndex,
                                 int32_t aNumOptions, int32_t aDoAdjustInc,
                                 int32_t aDoAdjustIncNext);

  /**
   * Resets the select back to it's original default values;
   * those values as determined by the original HTML
   */
  MOZ_CAN_RUN_SCRIPT void ResetList(bool aAllowScrolling);

  explicit nsListControlFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);
  virtual ~nsListControlFrame();

  /**
   * Sets the mSelectedIndex and mOldSelectedIndex from figuring out what
   * item was selected using content
   * @param aPoint the event point, in listcontrolframe coordinates
   * @return NS_OK if it successfully found the selection
   */
  nsresult GetIndexFromDOMEvent(mozilla::dom::Event* aMouseEvent,
                                int32_t& aCurIndex);

  bool CheckIfAllFramesHere();
  bool IsLeftButton(mozilla::dom::Event* aMouseEvent);

  // guess at a row block size based on our own style.
  nscoord CalcFallbackRowBSize(float aFontSizeInflation);

  // CalcIntrinsicBSize computes our intrinsic block size (taking the
  // "size" attribute into account).  This should only be called in
  // non-dropdown mode.
  nscoord CalcIntrinsicBSize(nscoord aBSizeOfARow, int32_t aNumberOfOptions);

  // Dropped down stuff
  void SetComboboxItem(int32_t aIndex);

  /**
   * Method to reflow ourselves as a dropdown list.  This differs from
   * reflow as a listbox because the criteria for needing a second
   * pass are different.  This will be called from Reflow() as needed.
   */
  void ReflowAsDropdown(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus& aStatus);

  // Selection
  bool SetOptionsSelectedFromFrame(int32_t aStartIndex, int32_t aEndIndex,
                                   bool aValue, bool aClearAll);
  bool ToggleOptionSelectedFromFrame(int32_t aIndex);

  MOZ_CAN_RUN_SCRIPT
  bool SingleSelection(int32_t aClickedIndex, bool aDoToggle);
  bool ExtendedSelection(int32_t aStartIndex, int32_t aEndIndex,
                         bool aClearAll);
  MOZ_CAN_RUN_SCRIPT
  bool PerformSelection(int32_t aClickedIndex, bool aIsShift, bool aIsControl);
  MOZ_CAN_RUN_SCRIPT
  bool HandleListSelection(mozilla::dom::Event* aDOMEvent,
                           int32_t selectedIndex);
  void InitSelectionRange(int32_t aClickedIndex);
  MOZ_CAN_RUN_SCRIPT
  void PostHandleKeyEvent(int32_t aNewIndex, uint32_t aCharCode, bool aIsShift,
                          bool aIsControlOrMeta);

 public:
  nsSelectsAreaFrame* GetOptionsContainer() const {
    return static_cast<nsSelectsAreaFrame*>(GetScrolledFrame());
  }

 protected:
  nscoord BSizeOfARow() { return GetOptionsContainer()->BSizeOfARow(); }

  /**
   * @return how many displayable options/optgroups this frame has.
   */
  uint32_t GetNumberOfRows();

  nsView* GetViewInternal() const override { return mView; }
  void SetViewInternal(nsView* aView) override { mView = aView; }

  // Data Members
  int32_t mStartSelectionIndex;
  int32_t mEndSelectionIndex;

  nsComboboxControlFrame* mComboboxFrame;

  // The view is only created (& non-null) if IsInDropDownMode() is true.
  nsView* mView;

  uint32_t mNumDisplayRows;
  bool mChangesSinceDragStart : 1;
  bool mButtonDown : 1;

  // Has the user selected a visible item since we showed the dropdown?
  bool mItemSelectionStarted : 1;

  bool mIsAllContentHere : 1;
  bool mIsAllFramesHere : 1;
  bool mHasBeenInitialized : 1;
  bool mNeedToReset : 1;
  bool mPostChildrenLoadedReset : 1;

  // bool value for multiple discontiguous selection
  bool mControlSelectMode : 1;

  // True if we're in the middle of a reflow and might need a second
  // pass.  This only happens for auto heights.
  bool mMightNeedSecondPass : 1;

  /**
   * Set to aPresContext->HasPendingInterrupt() at the start of Reflow.
   * Set to false at the end of DidReflow.
   */
  bool mHasPendingInterruptAtStartOfReflow : 1;

  // True if the drop-down can show more rows.  Always false if this list
  // is not in drop-down mode.
  bool mDropdownCanGrow : 1;

  // True if the selection can be set to nothing or disabled options.
  bool mForceSelection : 1;

  // The last computed block size we reflowed at if we're a combobox
  // dropdown.
  // XXXbz should we be using a subclass here?  Or just not worry
  // about the extra member on listboxes?
  nscoord mLastDropdownComputedBSize;

  // At the time of our last dropdown, the backstop color to draw in case we
  // are translucent.
  nscolor mLastDropdownBackstopColor;

  RefPtr<nsListEventListener> mEventListener;

  static nsListControlFrame* mFocused;
  static nsString* sIncrementalString;

#ifdef DO_REFLOW_COUNTER
  int32_t mReflowId;
#endif

 private:
  // for incremental typing navigation
  static nsAString& GetIncrementalString();
  static DOMTimeStamp gLastKeyTime;

  class MOZ_RAII AutoIncrementalSearchResetter {
   public:
    AutoIncrementalSearchResetter() : mCancelled(false) {}
    ~AutoIncrementalSearchResetter() {
      if (!mCancelled) {
        nsListControlFrame::GetIncrementalString().Truncate();
      }
    }
    void Cancel() { mCancelled = true; }

   private:
    bool mCancelled;
  };
};

#endif /* nsListControlFrame_h___ */
