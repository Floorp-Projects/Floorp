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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dean Tessman <dean_tessman@hotmail.com>
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

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsVoidArray.h"
#include "nsIAnonymousContentCreator.h"
#include "nsISelectControlFrame.h"
#include "nsIRollupListener.h"
#include "nsIPresState.h"
#include "nsCSSFrameConstructor.h"
#include "nsITextContent.h"
#include "nsIScrollableViewProvider.h"
#include "nsIStatefulFrame.h"
#include "nsIDOMMouseListener.h"

class nsIView;
class nsStyleContext;
class nsIListControlFrame;
class nsIScrollableView;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_COMBO_FRAME_POPUP_LIST_INDEX   (NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX + 1)

class nsComboboxControlFrame : public nsAreaFrame,
                               public nsIFormControlFrame,
                               public nsIComboboxControlFrame,
                               public nsIAnonymousContentCreator,
                               public nsISelectControlFrame,
                               public nsIRollupListener,
                               public nsIScrollableViewProvider,
                               public nsIStatefulFrame
{
public:
  friend nsresult NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags);

  nsComboboxControlFrame();
  ~nsComboboxControlFrame();

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  
   // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aChildList);
  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

   // nsIFrame
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext,
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                               nsIAtom*        aListName,
                               nsIFrame*       aChildList);
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

  virtual nsIFrame* GetContentInsertionFrame();

     // nsIFormControlFrame
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD GetName(nsAString* aName);
  NS_IMETHOD_(PRInt32) GetFormControlType() const;
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAString& aValue); 
  void       SetFocus(PRBool aOn, PRBool aRepaint);
  void       ScrollIntoView(nsIPresContext* aPresContext);
  virtual void InitializeControl(nsIPresContext* aPresContext);
  NS_IMETHOD OnContentReset();

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                     const nsFont*&  aFont);
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  // nsIFormMouseListener
  virtual void MouseClicked(nsIPresContext* aPresContext);

  //nsIComboboxControlFrame
  NS_IMETHOD IsDroppedDown(PRBool * aDoDropDown) { *aDoDropDown = mDroppedDown; return NS_OK; }
  NS_IMETHOD ShowDropDown(PRBool aDoDropDown);
  NS_IMETHOD GetDropDown(nsIFrame** aDropDownFrame);
  NS_IMETHOD SetDropDown(nsIFrame* aDropDownFrame);
  NS_IMETHOD RollupFromList(nsIPresContext* aPresContext);
  NS_IMETHOD AbsolutelyPositionDropDown();
  NS_IMETHOD GetAbsoluteRect(nsRect* aRect);
  NS_IMETHOD GetIndexOfDisplayArea(PRInt32* aSelectedIndex);
  NS_IMETHOD RedisplaySelectedText();
  NS_IMETHOD_(PRBool) NeededToFireOnChange();

  // nsISelectControlFrame
  NS_IMETHOD AddOption(nsIPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD RemoveOption(nsIPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD GetOptionSelected(PRInt32 aIndex, PRBool* aValue);
  NS_IMETHOD DoneAddingChildren(PRBool aIsDone);
  NS_IMETHOD OnOptionSelected(nsIPresContext* aPresContext,
                              PRInt32 aIndex,
                              PRBool aSelected);
  NS_IMETHOD OnOptionTextChanged(nsIDOMHTMLOptionElement* option);
  NS_IMETHOD GetDummyFrame(nsIFrame** aFrame);
  NS_IMETHOD SetDummyFrame(nsIFrame* aFrame);

  //nsIRollupListener
  // NS_DECL_NSIROLLUPLISTENER
  NS_IMETHOD Rollup();
   // a combobox should roll up if a mousewheel event happens outside of
   // the popup area
  NS_IMETHOD ShouldRollupOnMouseWheelEvent(PRBool *aShouldRollup)
    { *aShouldRollup = PR_TRUE; return NS_OK;}
  //NS_IMETHOD ShouldRollupOnMouseWheelEvent(nsIWidget *aWidget, PRBool *aShouldRollup) 
  //{ *aShouldRollup = PR_FALSE; return NS_OK;}

  // a combobox should not roll up if activated by a mouse activate message (eg. X-mouse)
  NS_IMETHOD ShouldRollupOnMouseActivate(PRBool *aShouldRollup)
    { *aShouldRollup = PR_FALSE; return NS_OK;}

  NS_IMETHOD SetFrameConstructor(nsCSSFrameConstructor *aConstructor)
    { mFrameConstructor = aConstructor; return NS_OK;} // not owner - do not addref!

  // nsIScrollableViewProvider
  NS_IMETHOD GetScrollableView(nsIPresContext* aPresContext, nsIScrollableView** aView);

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:

  NS_IMETHOD CreateDisplayFrame(nsIPresContext* aPresContext);

#ifdef DO_NEW_REFLOW
  NS_IMETHOD ReflowItems(nsIPresContext* aPresContext,
                         const nsHTMLReflowState& aReflowState,
                         nsHTMLReflowMetrics& aDesiredSize);
#endif

   // nsHTMLContainerFrame
  virtual PRIntn GetSkipSides() const;

   // Utilities
  nsresult ReflowComboChildFrame(nsIFrame*           aFrame, 
                            nsIPresContext*          aPresContext, 
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState, 
                            nsReflowStatus&          aStatus,
                            nscoord                  aAvailableWidth,
                            nscoord                  aAvailableHeight);

public:
  nsresult PositionDropdown(nsIPresContext* aPresContext,
                            nscoord aHeight, 
                            nsRect aAbsoluteTwipsRect, 
                            nsRect aAbsolutePixelRect);
protected:
  void ShowPopup(PRBool aShowPopup);
  void ShowList(nsIPresContext* aPresContext, PRBool aShowList);
  void SetChildFrameSize(nsIFrame* aFrame, nscoord aWidth, nscoord aHeight);
  void InitTextStr();
  void CheckFireOnChange();
  nsresult RedisplayText(PRInt32 aIndex);
  nsresult ActuallyDisplayText(nsAString& aText, PRBool aNotify);
  nsresult GetPrimaryComboFrame(nsIPresContext* aPresContext, nsIContent* aContent, nsIFrame** aFrame);
  NS_IMETHOD ToggleList(nsIPresContext* aPresContext);

  void ReflowCombobox(nsIPresContext *         aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      nsReflowStatus&          aStatus,
                      nsIFrame *               aDisplayFrame,
                      nsIFrame *               aDropDownBtn,
                      nscoord&                 aDisplayWidth,
                      nscoord                  aBtnWidth,
                      const nsMargin&          aBorderPadding,
                      nscoord                  aFallBackHgt = -1,
                      PRBool                   aCheckHeight = PR_FALSE);

  nsFrameList              mPopupFrames;             // additional named child list
  nsIPresContext*          mPresContext;             // XXX: Remove the need to cache the pres context.
  nsCOMPtr<nsITextContent> mDisplayContent;          // Anonymous content used to display the current selection
  nsIFrame*                mDisplayFrame;            // frame to display selection
  nsIFrame*                mButtonFrame;             // button frame
  nsIFrame*                mDropdownFrame;           // dropdown list frame
  nsIFrame*                mTextFrame;               // display area frame
  nsIListControlFrame *    mListControlFrame;        // ListControl Interface for the dropdown frame

  // Resize Reflow Optimization
  nsSize                mCacheSize;
  nsSize                mCachedAvailableSize;
  nscoord               mCachedMaxElementWidth;
  nscoord               mCachedAscent;

  nsSize                mCachedUncDropdownSize;
  nsSize                mCachedUncComboSize;

  nscoord               mItemDisplayWidth;
  //nscoord               mItemDisplayHeight;
  nsCSSFrameConstructor* mFrameConstructor;

  PRPackedBool          mDroppedDown;             // Current state of the dropdown list, PR_TRUE is dropped down
  PRPackedBool          mGoodToGo;

  PRInt32               mRecentSelectedIndex;
  PRInt32               mDisplayedIndex;

  // make someone to listen to the button. If its programmatically pressed by someone like Accessibility
  // then open or close the combo box.
  nsCOMPtr<nsIDOMMouseListener> mButtonListener;

  // static class data member for Bug 32920
  // only one control can be focused at a time
  static nsComboboxControlFrame * mFocused;

#ifdef DO_REFLOW_COUNTER
  PRInt32 mReflowId;
#endif

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif


