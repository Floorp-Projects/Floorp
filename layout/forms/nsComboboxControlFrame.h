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

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsISelectControlFrame.h"
#include "nsIRollupListener.h"
#include "nsPresState.h"
#include "nsCSSFrameConstructor.h"
#include "nsIScrollableViewProvider.h"
#include "nsIStatefulFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsThreadUtils.h"

class nsIView;
class nsStyleContext;
class nsIListControlFrame;
class nsIScrollableView;
class nsComboboxDisplayFrame;

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
  friend nsIFrame* NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);
  friend class nsComboboxDisplayFrame;

  nsComboboxControlFrame(nsStyleContext* aContext);
  ~nsComboboxControlFrame();

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);

  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PaintFocus(nsIRenderingContext& aRenderingContext, nsPoint aPt);

  // XXXbz this is only needed to prevent the quirk percent height stuff from
  // leaking out of the combobox.  We may be able to get rid of this as more
  // things move to IsFrameOfType.
  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  virtual void Destroy();
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;

  virtual nsIFrame* GetContentInsertionFrame();

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 
  virtual void SetFocus(PRBool aOn, PRBool aRepaint);

  //nsIComboboxControlFrame
  virtual PRBool IsDroppedDown() { return mDroppedDown; }
  virtual void ShowDropDown(PRBool aDoDropDown);
  virtual nsIFrame* GetDropDown();
  virtual void SetDropDown(nsIFrame* aDropDownFrame);
  virtual void RollupFromList();
  virtual void AbsolutelyPositionDropDown();
  virtual PRInt32 GetIndexOfDisplayArea();
  NS_IMETHOD RedisplaySelectedText();
  virtual PRInt32 UpdateRecentIndex(PRInt32 aIndex);
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

  // nsIScrollableViewProvider
  virtual nsIScrollableView* GetScrollableView();

  //nsIStatefulFrame
  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState);
  NS_IMETHOD RestoreState(nsPresState* aState);

  static PRBool ToolkitHasNativePopup();

protected:

  // Utilities
  nsresult ReflowDropdown(nsPresContext*          aPresContext, 
                          const nsHTMLReflowState& aReflowState);

protected:
  class RedisplayTextEvent;
  friend class RedisplayTextEvent;

  class RedisplayTextEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    RedisplayTextEvent(nsComboboxControlFrame *c) : mControlFrame(c) {}
    void Revoke() { mControlFrame = nsnull; }
  private:
    nsComboboxControlFrame *mControlFrame;
  };
  
  void ShowPopup(PRBool aShowPopup);
  void ShowList(nsPresContext* aPresContext, PRBool aShowList);
  void CheckFireOnChange();
  void FireValueChangeEvent();
  nsresult RedisplayText(PRInt32 aIndex);
  void HandleRedisplayTextEvent();
  void ActuallyDisplayText(PRBool aNotify);
  NS_IMETHOD ToggleList(nsPresContext* aPresContext);

  nsFrameList              mPopupFrames;             // additional named child list
  nsCOMPtr<nsIContent>     mDisplayContent;          // Anonymous content used to display the current selection
  nsCOMPtr<nsIContent>     mButtonContent;           // Anonymous content for the button
  nsIFrame*                mDisplayFrame;            // frame to display selection
  nsIFrame*                mButtonFrame;             // button frame
  nsIFrame*                mDropdownFrame;           // dropdown list frame
  nsIFrame*                mTextFrame;               // display area frame
  nsIListControlFrame *    mListControlFrame;        // ListControl Interface for the dropdown frame

  // The width of our display area.  Used by that frame's reflow to
  // size to the full width except the drop-marker.
  nscoord mDisplayWidth;
  
  PRPackedBool          mDroppedDown;             // Current state of the dropdown list, PR_TRUE is dropped down
  PRPackedBool          mInRedisplayText;

  nsRevocableEventPtr<RedisplayTextEvent> mRedisplayTextEvent;

  PRInt32               mRecentSelectedIndex;
  PRInt32               mDisplayedIndex;
  nsString              mDisplayedOptionText;

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
