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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Mike Pinkerton (pinkerton@netscape.com)
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

//
// nsMenuPopupFrame
//

#ifndef nsMenuPopupFrame_h__
#define nsMenuPopupFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventReceiver.h"
#include "nsMenuListener.h"

#include "nsBoxFrame.h"
#include "nsIMenuParent.h"
#include "nsIWidget.h"

#include "nsITimer.h"

#define INC_TYP_INTERVAL  1000  // 1s. If the interval between two keypresses is shorter than this, 
                                //   treat as a continue typing
// XXX, kyle.yuan@sun.com, there are 4 definitions for the same purpose:
//  nsMenuPopupFrame.h, nsListControlFrame.cpp, listbox.xml, tree.xml
//  need to find a good place to put them together.
//  if someone changes one, please also change the other.

nsresult NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;

class nsIViewManager;
class nsIView;
class nsIMenuParent;
class nsIMenuFrame;
class nsIDOMXULDocument;


class nsMenuPopupFrame : public nsBoxFrame, public nsIMenuParent, public nsITimerCallback
{
public:
  nsMenuPopupFrame(nsIPresShell* aShell);

  NS_DECL_ISUPPORTS

  // The nsITimerCallback interface
  NS_DECL_NSITIMERCALLBACK

  // nsIMenuParentInterface
  NS_IMETHOD GetCurrentMenuItem(nsIMenuFrame** aResult);
  NS_IMETHOD SetCurrentMenuItem(nsIMenuFrame* aMenuItem);
  NS_IMETHOD GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD SetActive(PRBool aActiveFlag) { return NS_OK; }; // We don't care.
  NS_IMETHOD GetIsActive(PRBool& isActive) { isActive = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsMenuBar(PRBool& isMenuBar) { isMenuBar = PR_FALSE; return NS_OK; };
  NS_IMETHOD ConsumeOutsideClicks(PRBool& aConsumeOutsideClicks);
  NS_IMETHOD ClearRecentlyRolledUp() {return NS_OK;}
  NS_IMETHOD RecentlyRolledUp(nsIMenuFrame *aMenuFrame, PRBool *aJustRolledUp) {*aJustRolledUp = PR_FALSE; return NS_OK;}
  NS_IMETHOD SetIsContextMenu(PRBool aIsContextMenu) { mIsContextMenu = aIsContextMenu; return NS_OK; };
  NS_IMETHOD GetIsContextMenu(PRBool& aIsContextMenu) { aIsContextMenu = mIsContextMenu; return NS_OK; };
  
  NS_IMETHOD GetParentPopup(nsIMenuParent** aResult);

  // Closes up the chain of open cascaded menus.
  NS_IMETHOD DismissChain();

  // Hides the chain of cascaded menus without closing them up.
  NS_IMETHOD HideChain();

  NS_IMETHOD KillPendingTimers();

  NS_IMETHOD InstallKeyboardNavigator();
  NS_IMETHOD RemoveKeyboardNavigator();

  NS_IMETHOD GetWidget(nsIWidget **aWidget);

  // The dismissal listener gets created and attached to the window.
  NS_IMETHOD CreateDismissalListener();

  // Overridden methods
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  NS_IMETHOD MarkStyleChange(nsBoxLayoutState& aState);
  NS_IMETHOD MarkDirty(nsBoxLayoutState& aState);
  NS_IMETHOD RelayoutDirtyChild(nsBoxLayoutState& aState, nsIBox* aChild);

  void GetViewOffset(nsIView* aView, nsPoint& aPoint);
  static void GetRootViewForPopup(nsPresContext* aPresContext,
                                  nsIFrame* aStartFrame,
                                  PRBool aStopAtViewManagerRoot,
                                  nsIView** aResult);

  nsresult SyncViewWithFrame(nsPresContext* aPresContext, const nsString& aPopupAnchor,
                             const nsString& aPopupAlign,
                             nsIFrame* aFrame, PRInt32 aXPos, PRInt32 aYPos);

  NS_IMETHOD KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag);
  NS_IMETHOD ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag);
  
  NS_IMETHOD Escape(PRBool& aHandledFlag);
  NS_IMETHOD Enter();

  nsIMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, PRBool& doAction);

  PRBool IsValidItem(nsIContent* aContent);
  PRBool IsDisabled(nsIContent* aContent);

  void GetContextMenu(nsIMenuParent** aContextMenu);

  NS_IMETHOD KillCloseTimer();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuPopup"), aResult);
  }
#endif

  void EnsureMenuItemIsVisible(nsIMenuFrame* aMenuFrame);

  void MoveTo(PRInt32 aLeft, PRInt32 aTop);
  void GetScreenPosition(nsIView* aView, nsPoint& aScreenPosition);

  void GetAutoPosition(PRBool* aShouldAutoPosition);
  void SetAutoPosition(PRBool aShouldAutoPosition);
  void EnableRollup(PRBool aShouldRollup);

  nsIScrollableView* GetScrollableView(nsIFrame* aStart);
  
protected:
  // redefine to tell the box system not to move the
  // views.
  virtual void GetLayoutFlags(PRUint32& aFlags);

  // given x,y in client coordinates, compensate for nested documents like framesets.
  void AdjustClientXYForNestedDocuments ( nsIDOMXULDocument* inPopupDoc, nsIPresShell* inPopupShell, 
                                            PRInt32 inClientX, PRInt32 inClientY, 
                                            PRInt32* outAdjX, PRInt32* outAdjY ) ;

  void AdjustPositionForAnchorAlign ( PRInt32* ioXPos, PRInt32* ioYPos, const nsRect & inParentRect,
                                        const nsString& aPopupAnchor, const nsString& aPopupAlign,
                                        PRBool* outFlushWithTopBottom ) ;

  PRBool IsMoreRoomOnOtherSideOfParent ( PRBool inFlushAboveBelow, PRInt32 inScreenViewLocX, PRInt32 inScreenViewLocY,
                                           const nsRect & inScreenParentFrameRect, PRInt32 inScreenTopTwips, PRInt32 inScreenLeftTwips,
                                           PRInt32 inScreenBottomTwips, PRInt32 inScreenRightTwips ) ;

  void MovePopupToOtherSideOfParent ( PRBool inFlushAboveBelow, PRInt32* ioXPos, PRInt32* ioYPos, 
                                           PRInt32* ioScreenViewLocX, PRInt32* ioScreenViewLocY,
                                           const nsRect & inScreenParentFrameRect, PRInt32 inScreenTopTwips, PRInt32 inScreenLeftTwips,
                                           PRInt32 inScreenBottomTwips, PRInt32 inScreenRightTwips ) ;

  // Move the popup to the position specified in its |left| and |top| attributes.
  void MoveToAttributePosition();


  nsIMenuFrame* mCurrentMenu; // The current menu that is active.
  // XXX Hack
  nsPresContext* mPresContext;  // weak reference

  nsMenuListener* mKeyboardNavigator; // The listener that tells us about key events.
  nsIDOMEventReceiver* mTarget;

  nsIMenuFrame* mTimerMenu; // A menu awaiting closure.
  nsCOMPtr<nsITimer> mCloseTimer; // Close timer.

  PRPackedBool mIsContextMenu;  // is this a context menu?
  
  PRPackedBool mMenuCanOverlapOSBar;    // can we appear over the taskbar/menubar?

  PRPackedBool mShouldAutoPosition; // Should SyncViewWithFrame be allowed to auto position popup?
  PRPackedBool mShouldRollup; // Should this menupopup be allowed to dismiss automatically?

  nsString     mIncrementalString;  // for incremental typing navigation

}; // class nsMenuPopupFrame

#endif
