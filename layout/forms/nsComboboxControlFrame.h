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
 *   Mats Palmgren <matspal@gmail.com>
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

#include "nsBlockFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsISelectControlFrame.h"
#include "nsIRollupListener.h"
#include "nsPresState.h"
#include "nsCSSFrameConstructor.h"
#include "nsIStatefulFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMEventListener.h"
#include "nsThreadUtils.h"

class nsIView;
class nsStyleContext;
class nsIListControlFrame;
class nsComboboxDisplayFrame;

class nsComboboxControlFrame : public nsBlockFrame,
                               public nsIFormControlFrame,
                               public nsIComboboxControlFrame,
                               public nsIAnonymousContentCreator,
                               public nsISelectControlFrame,
                               public nsIRollupListener,
                               public nsIStatefulFrame
{
public:
  friend nsIFrame* NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);
  friend class nsComboboxDisplayFrame;

  nsComboboxControlFrame(nsStyleContext* aContext);
  ~nsComboboxControlFrame();

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter);
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);

  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

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

  void PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt);

  // XXXbz this is only needed to prevent the quirk percent height stuff from
  // leaking out of the combobox.  We may be able to get rid of this as more
  // things move to IsFrameOfType.
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual nsIScrollableFrame* GetScrollTargetFrame() {
    return do_QueryFrame(mDropdownFrame);
  }

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);
  virtual nsFrameList GetChildList(ChildListID aListID) const;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const;

  virtual nsIFrame* GetContentInsertionFrame();

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 
  /**
   * Inform the control that it got (or lost) focus.
   * If it lost focus, the dropdown menu will be rolled up if needed,
   * and FireOnChange() will be called.
   * @param aOn true if got focus, false if lost focus.
   * @param aRepaint if true then force repaint (NOTE: we always force repaint currently)
   * @note This method might destroy |this|.
   */
  virtual void SetFocus(bool aOn, bool aRepaint);

  //nsIComboboxControlFrame
  virtual bool IsDroppedDown() { return mDroppedDown; }
  /**
   * @note This method might destroy |this|.
   */
  virtual void ShowDropDown(bool aDoDropDown);
  virtual nsIFrame* GetDropDown();
  virtual void SetDropDown(nsIFrame* aDropDownFrame);
  /**
   * @note This method might destroy |this|.
   */
  virtual void RollupFromList();
  virtual void AbsolutelyPositionDropDown();
  virtual PRInt32 GetIndexOfDisplayArea();
  /**
   * @note This method might destroy |this|.
   */
  NS_IMETHOD RedisplaySelectedText();
  virtual PRInt32 UpdateRecentIndex(PRInt32 aIndex);
  virtual void OnContentReset();

  // nsISelectControlFrame
  NS_IMETHOD AddOption(PRInt32 index);
  NS_IMETHOD RemoveOption(PRInt32 index);
  NS_IMETHOD DoneAddingChildren(bool aIsDone);
  NS_IMETHOD OnOptionSelected(PRInt32 aIndex, bool aSelected);
  NS_IMETHOD OnSetSelectedIndex(PRInt32 aOldIndex, PRInt32 aNewIndex);

  //nsIRollupListener
  /**
   * Hide the dropdown menu and stop capturing mouse events.
   * @note This method might destroy |this|.
   */
  virtual nsIContent* Rollup(PRUint32 aCount, bool aGetLastRolledUp = false);

  /**
   * A combobox should roll up if a mousewheel event happens outside of
   * the popup area.
   */
  virtual bool ShouldRollupOnMouseWheelEvent()
    { return true; }

  /**
   * A combobox should not roll up if activated by a mouse activate message
   * (eg. X-mouse).
   */
  virtual bool ShouldRollupOnMouseActivate()
    { return false; }

  virtual PRUint32 GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain)
    { return 0; }

  //nsIStatefulFrame
  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState);
  NS_IMETHOD RestoreState(nsPresState* aState);

  static bool ToolkitHasNativePopup();

protected:

  // Utilities
  nsresult ReflowDropdown(nsPresContext*          aPresContext, 
                          const nsHTMLReflowState& aReflowState);

  // Helper for GetMinWidth/GetPrefWidth
  nscoord GetIntrinsicWidth(nsRenderingContext* aRenderingContext,
                            nsLayoutUtils::IntrinsicWidthType aType);
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
  nsresult RedisplayText(PRInt32 aIndex);
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
  
  bool                  mDroppedDown;             // Current state of the dropdown list, true is dropped down
  bool                  mInRedisplayText;

  nsRevocableEventPtr<RedisplayTextEvent> mRedisplayTextEvent;

  PRInt32               mRecentSelectedIndex;
  PRInt32               mDisplayedIndex;
  nsString              mDisplayedOptionText;

  // make someone to listen to the button. If its programmatically pressed by someone like Accessibility
  // then open or close the combo box.
  nsCOMPtr<nsIDOMEventListener> mButtonListener;

  // static class data member for Bug 32920
  // only one control can be focused at a time
  static nsComboboxControlFrame * mFocused;

#ifdef DO_REFLOW_COUNTER
  PRInt32 mReflowId;
#endif
};

#endif
