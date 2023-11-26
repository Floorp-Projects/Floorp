/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsListControlFrame_h___
#define nsListControlFrame_h___

#ifdef DEBUG_evaughan
// #define DEBUG_rods
#endif

#ifdef DEBUG_rods
// #define DO_REFLOW_DEBUG
// #define DO_REFLOW_COUNTER
// #define DO_UNCONSTRAINED_CHECK
// #define DO_PIXELS
#endif

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsGfxScrollFrame.h"
#include "nsIFormControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsSelectsAreaFrame.h"

class nsComboboxControlFrame;
class nsPresContext;

namespace mozilla {
class PresShell;
class HTMLSelectEventListener;

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

  friend nsListControlFrame* NS_NewListControlFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsListControlFrame)

  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext) const override;

  // nsIFrame
  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) final;

  void SetInitialChildList(ChildListID aListID, nsFrameList&& aChildList) final;

  nscoord GetPrefISize(gfxContext* aRenderingContext) final;
  nscoord GetMinISize(gfxContext* aRenderingContext) final;

  void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) final;

  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) final;
  void Destroy(DestroyContext&) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final;

  nsContainerFrame* GetContentInsertionFrame() final;

  int32_t GetEndSelectionIndex() const { return mEndSelectionIndex; }

  mozilla::dom::HTMLOptionElement* GetCurrentOption() const;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const final;
#endif

  // nsIFormControlFrame
  nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) final;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SetFocus(bool aOn = true, bool aRepaint = false) final;

  bool ShouldPropagateComputedBSizeToScrolledContent() const final;

  // for accessibility purposes
#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() final;
#endif

  int32_t GetSelectedIndex();

  /**
   * Gets the text of the currently selected item.
   * If the there are zero items then an empty string is returned
   * If there is nothing selected, then the 0th item's text is returned.
   */
  void GetOptionText(uint32_t aIndex, nsAString& aStr);

  void CaptureMouseEvents(bool aGrabMouseEvents);
  nscoord GetBSizeOfARow();
  uint32_t GetNumberOfOptions();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void OnContentReset();

  // nsISelectControlFrame
  NS_IMETHOD AddOption(int32_t index) final;
  NS_IMETHOD RemoveOption(int32_t index) final;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD DoneAddingChildren(bool aIsDone) final;

  /**
   * Gets the content (an option) by index and then set it as
   * being selected or not selected.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD OnOptionSelected(int32_t aIndex, bool aSelected) final;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD_(void)
  OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) final;

  /**
   * Mouse event listeners.
   * @note These methods might destroy the frame, pres shell and other objects.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult HandleLeftButtonMouseDown(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult HandleLeftButtonMouseUp(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult DragMove(mozilla::dom::Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT

  MOZ_CAN_RUN_SCRIPT
  bool PerformSelection(int32_t aClickedIndex, bool aIsShift, bool aIsControl);
  MOZ_CAN_RUN_SCRIPT
  void UpdateSelectionAfterKeyEvent(int32_t aNewIndex, uint32_t aCharCode,
                                    bool aIsShift, bool aIsControlOrMeta,
                                    bool aIsControlSelectMode);

  /**
   * Returns the options collection for mContent, if any.
   */
  mozilla::dom::HTMLOptionsCollection* GetOptions() const;
  /**
   * Returns the HTMLOptionElement for a given index in mContent's collection.
   */
  HTMLOptionElement* GetOption(uint32_t aIndex) const;

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
   * Return the number of displayed rows in the list.
   */
  uint32_t GetNumDisplayRows() const { return mNumDisplayRows; }

#ifdef ACCESSIBILITY
  /**
   * Post a custom DOM event for the change, so that accessibility can
   * fire a native focus event for accessibility
   * (Some 3rd party products need to track our focus)
   */
  void FireMenuItemActiveEvent(
      nsIContent* aPreviousOption);  // Inform assistive tech what got focused
#endif

 protected:
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
    return mContent->AsElement()->HasAttr(nsGkAtoms::multiple);
  }

  mozilla::dom::HTMLSelectElement& Select() const;

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

 public:
  /**
   * Resets the select back to it's original default values;
   * those values as determined by the original HTML
   */
  MOZ_CAN_RUN_SCRIPT void ResetList(bool aAllowScrolling);

 protected:
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

  // guess at a row block size based on our own style.
  nscoord CalcFallbackRowBSize(float aFontSizeInflation);

  // CalcIntrinsicBSize computes our intrinsic block size (taking the
  // "size" attribute into account).  This should only be called in
  // non-dropdown mode.
  nscoord CalcIntrinsicBSize(nscoord aBSizeOfARow, int32_t aNumberOfOptions);

  // Dropped down stuff
  void SetComboboxItem(int32_t aIndex);

  // Selection
  bool SetOptionsSelectedFromFrame(int32_t aStartIndex, int32_t aEndIndex,
                                   bool aValue, bool aClearAll);
  bool ToggleOptionSelectedFromFrame(int32_t aIndex);

  MOZ_CAN_RUN_SCRIPT
  bool SingleSelection(int32_t aClickedIndex, bool aDoToggle);
  bool ExtendedSelection(int32_t aStartIndex, int32_t aEndIndex,
                         bool aClearAll);
  MOZ_CAN_RUN_SCRIPT
  bool HandleListSelection(mozilla::dom::Event* aDOMEvent,
                           int32_t selectedIndex);
  void InitSelectionRange(int32_t aClickedIndex);

 public:
  nsSelectsAreaFrame* GetOptionsContainer() const {
    return static_cast<nsSelectsAreaFrame*>(GetScrolledFrame());
  }

  static constexpr int32_t kNothingSelected = -1;

 protected:
  nscoord BSizeOfARow() { return GetOptionsContainer()->BSizeOfARow(); }

  /**
   * @return how many displayable options/optgroups this frame has.
   */
  uint32_t GetNumberOfRows();

  // Data Members
  int32_t mStartSelectionIndex;
  int32_t mEndSelectionIndex;

  uint32_t mNumDisplayRows;
  bool mChangesSinceDragStart : 1;

  // Has the user selected a visible item since we showed the dropdown?
  bool mItemSelectionStarted : 1;

  bool mIsAllContentHere : 1;
  bool mIsAllFramesHere : 1;
  bool mHasBeenInitialized : 1;
  bool mNeedToReset : 1;
  bool mPostChildrenLoadedReset : 1;

  // True if we're in the middle of a reflow and might need a second
  // pass.  This only happens for auto heights.
  bool mMightNeedSecondPass : 1;

  /**
   * Set to aPresContext->HasPendingInterrupt() at the start of Reflow.
   * Set to false at the end of DidReflow.
   */
  bool mHasPendingInterruptAtStartOfReflow : 1;

  // True if the selection can be set to nothing or disabled options.
  bool mForceSelection : 1;

  RefPtr<mozilla::HTMLSelectEventListener> mEventListener;

  static nsListControlFrame* mFocused;

#ifdef DO_REFLOW_COUNTER
  int32_t mReflowId;
#endif
};

#endif /* nsListControlFrame_h___ */
