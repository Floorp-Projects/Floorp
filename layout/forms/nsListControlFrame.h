/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

#include "nsGfxScrollFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMEventListener.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsSelectsAreaFrame.h"

class nsIDOMHTMLSelectElement;
class nsIDOMHTMLOptionsCollection;
class nsIDOMHTMLOptionElement;
class nsIComboboxControlFrame;
class nsPresContext;
class nsVoidArray;

class nsVoidArray;
class nsListEventListener;

/**
 * Frame-based listbox.
 */

class nsListControlFrame : public nsHTMLScrollFrame,
                           public nsIFormControlFrame, 
                           public nsIListControlFrame,
                           public nsISelectControlFrame
{
public:
  friend nsIFrame* NS_NewListControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsIFrame
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);
  
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  // Our min width is our pref width
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        aPrevInFlow);

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext, 
                       const nsHTMLReflowState*  aReflowState, 
                       nsDidReflowStatus         aStatus);
  virtual void Destroy();

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nsIFrame* GetContentInsertionFrame();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsHTMLScrollFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual PRBool IsContainingBlock() const;

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRBool aImmediate);

#ifdef DEBUG
    // nsIFrameDebug
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

    // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 
  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);

  virtual nsGfxScrollFrameInner::ScrollbarStyles GetScrollbarStyles() const;

    // for accessibility purposes
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

    // nsHTMLContainerFrame
  virtual PRIntn GetSkipSides() const;

    // nsIListControlFrame
  virtual void SetComboboxFrame(nsIFrame* aComboboxFrame);
  virtual PRInt32 GetSelectedIndex(); 
  virtual void GetOptionText(PRInt32 aIndex, nsAString & aStr);
  virtual void CaptureMouseEvents(PRBool aGrabMouseEvents);
  virtual nscoord GetHeightOfARow();
  virtual PRInt32 GetNumberOfOptions();  
  virtual void SyncViewWithFrame();
  virtual void AboutToDropDown();
  virtual void AboutToRollup();
  virtual void FireOnChange();
  virtual void ComboboxFinish(PRInt32 aIndex);
  virtual void OnContentReset();

  // nsISelectControlFrame
  NS_IMETHOD AddOption(nsPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD RemoveOption(nsPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD GetOptionSelected(PRInt32 aIndex, PRBool* aValue);
  NS_IMETHOD DoneAddingChildren(PRBool aIsDone);
  NS_IMETHOD OnOptionSelected(nsPresContext* aPresContext,
                              PRInt32 aIndex,
                              PRBool aSelected);
  NS_IMETHOD OnSetSelectedIndex(PRInt32 aOldIndex, PRInt32 aNewIndex);

  // mouse event listeners
  nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  nsresult MouseUp(nsIDOMEvent* aMouseEvent);

  // mouse motion listeners
  nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  nsresult DragMove(nsIDOMEvent* aMouseEvent);

  // key listeners
  nsresult KeyPress(nsIDOMEvent* aKeyEvent);

  // Static Methods
  static already_AddRefed<nsIDOMHTMLOptionsCollection>
    GetOptions(nsIContent * aContent);
  static already_AddRefed<nsIDOMHTMLOptionElement>
    GetOption(nsIDOMHTMLOptionsCollection* aOptions, PRInt32 aIndex);
  static already_AddRefed<nsIContent>
    GetOptionAsContent(nsIDOMHTMLOptionsCollection* aCollection,PRInt32 aIndex);

  static void ComboboxFocusSet();

  // Helper
  PRBool IsFocused() { return this == mFocused; }

  /**
   * Function to paint the focus rect when our nsSelectsAreaFrame is painting.
   * @param aPt the offset of this frame, relative to the rendering reference
   * frame
   */
  void PaintFocus(nsIRenderingContext& aRC, nsPoint aPt);

  /**
   * Function to calculate the height a row, for use with the "size" attribute.
   * Can't be const because GetNumberOfOptions() isn't const.
   */
  nscoord CalcHeightOfARow();

  /**
   * Function to ask whether we're currently in what might be the
   * first pass of a two-pass reflow.
   */
  PRBool MightNeedSecondPass() const {
    return mMightNeedSecondPass;
  }

  void SetSuppressScrollbarUpdate(PRBool aSuppress) {
    nsHTMLScrollFrame::SetSuppressScrollbarUpdate(aSuppress);
  }

  /**
   * Return whether the list is in dropdown mode.
   */
  PRBool IsInDropDownMode() const;

#ifdef ACCESSIBILITY
  void FireMenuItemActiveEvent(); // Inform assistive tech what got focused
#endif

protected:
  // Returns PR_FALSE if calling it destroyed |this|.
  PRBool     UpdateSelection();
  PRBool     GetMultiple(nsIDOMHTMLSelectElement* aSelect = nsnull) const;
  void       DropDownToggleKey(nsIDOMEvent* aKeyEvent);
  nsresult   IsOptionDisabled(PRInt32 anIndex, PRBool &aIsDisabled);
  nsresult   ScrollToFrame(nsIContent * aOptElement);
  nsresult   ScrollToIndex(PRInt32 anIndex);
  PRBool     IgnoreMouseEventForSelection(nsIDOMEvent* aEvent);
  void       UpdateInListState(nsIDOMEvent* aEvent);
  void       AdjustIndexForDisabledOpt(PRInt32 aStartIndex, PRInt32 &anNewIndex,
                                       PRInt32 aNumOptions, PRInt32 aDoAdjustInc, PRInt32 aDoAdjustIncNext);
  virtual void ResetList(PRBool aAllowScrolling);

  nsListControlFrame(nsIPresShell* aShell, nsIDocument* aDocument, nsStyleContext* aContext);
  virtual ~nsListControlFrame();

  // Utility methods
  nsresult GetSizeAttribute(PRInt32 *aSize);
  nsIContent* GetOptionFromContent(nsIContent *aContent);
  nsresult GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, PRInt32& aCurIndex);
  already_AddRefed<nsIContent> GetOptionContent(PRInt32 aIndex) const;
  PRBool   IsContentSelected(nsIContent* aContent) const;
  PRBool   IsContentSelectedByIndex(PRInt32 aIndex) const;
  PRBool   IsOptionElement(nsIContent* aContent);
  PRBool   CheckIfAllFramesHere();
  PRInt32  GetIndexFromContent(nsIContent *aContent);
  PRBool   IsLeftButton(nsIDOMEvent* aMouseEvent);

  // aNumOptions is the number of options we have; if we have none,
  // we'll just guess at a row height based on our own style.
  nscoord  CalcFallbackRowHeight(PRInt32 aNumOptions);

  // CalcIntrinsicHeight computes our intrinsic height (taking the "size"
  // attribute into account).  This should only be called in non-dropdown mode.
  nscoord CalcIntrinsicHeight(nscoord aHeightOfARow, PRInt32 aNumberOfOptions);

  // Dropped down stuff
  void     SetComboboxItem(PRInt32 aIndex);

  /**
   * Method to reflow ourselves as a dropdown list.  This differs from
   * reflow as a listbox because the criteria for needing a second
   * pass are different.  This will be called from Reflow() as needed.
   */
  nsresult ReflowAsDropdown(nsPresContext*           aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus);

  // Selection
  PRBool   SetOptionsSelectedFromFrame(PRInt32 aStartIndex,
                                       PRInt32 aEndIndex,
                                       PRBool aValue,
                                       PRBool aClearAll);
  PRBool   ToggleOptionSelectedFromFrame(PRInt32 aIndex);
  PRBool   SingleSelection(PRInt32 aClickedIndex, PRBool aDoToggle);
  PRBool   ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex,
                             PRBool aClearAll);
  PRBool   PerformSelection(PRInt32 aClickedIndex, PRBool aIsShift,
                            PRBool aIsControl);
  PRBool   HandleListSelection(nsIDOMEvent * aDOMEvent, PRInt32 selectedIndex);
  void     InitSelectionRange(PRInt32 aClickedIndex);

  nsSelectsAreaFrame* GetOptionsContainer() const {
    return NS_STATIC_CAST(nsSelectsAreaFrame*, GetScrolledFrame());
  }

  nscoord HeightOfARow() {
    return GetOptionsContainer()->HeightOfARow();
  }
  
  // Data Members
  PRInt32      mStartSelectionIndex;
  PRInt32      mEndSelectionIndex;

  nsIComboboxControlFrame *mComboboxFrame;
  PRInt32      mNumDisplayRows;
  PRPackedBool mChangesSinceDragStart:1;
  PRPackedBool mButtonDown:1;
  // Has the user selected a visible item since we showed the
  // dropdown?
  PRPackedBool mItemSelectionStarted:1;

  PRPackedBool mIsAllContentHere:1;
  PRPackedBool mIsAllFramesHere:1;
  PRPackedBool mHasBeenInitialized:1;
  PRPackedBool mNeedToReset:1;
  PRPackedBool mPostChildrenLoadedReset:1;

  //bool value for multiple discontiguous selection
  PRPackedBool mControlSelectMode:1;

  // True if we're in the middle of a reflow and might need a second
  // pass.  This only happens for auto heights.
  PRPackedBool mMightNeedSecondPass:1;

  // The last computed height we reflowed at if we're a combobox dropdown.
  // XXXbz should we be using a subclass here?  Or just not worry
  // about the extra member on listboxes?
  nscoord mLastDropdownComputedHeight;

  nsRefPtr<nsListEventListener> mEventListener;

  static nsListControlFrame * mFocused;
  
#ifdef DO_REFLOW_COUNTER
  PRInt32 mReflowId;
#endif

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // for incremental typing navigation
  static nsAString& GetIncrementalString ();
  static DOMTimeStamp gLastKeyTime;
};

#endif /* nsListControlFrame_h___ */

