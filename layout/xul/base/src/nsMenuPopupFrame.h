/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s):
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 */

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
#include "nsITimerCallback.h"

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
  NS_IMETHOD_(void) Notify(nsITimer *aTimer);

  // nsIMenuParentInterface
  NS_IMETHOD GetCurrentMenuItem(nsIMenuFrame** aResult);
  NS_IMETHOD SetCurrentMenuItem(nsIMenuFrame* aMenuItem);
  NS_IMETHOD GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD SetActive(PRBool aActiveFlag) { return NS_OK; }; // We don't care.
  NS_IMETHOD GetIsActive(PRBool& isActive) { isActive = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsMenuBar(PRBool& isMenuBar) { isMenuBar = PR_FALSE; return NS_OK; };
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
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  void GetViewOffset(nsIViewManager* aManager, nsIView* aView, nsPoint& aPoint);
  static void GetNearestEnclosingView(nsIPresContext* aPresContext, nsIFrame* aStartFrame, nsIView** aResult);

  nsresult SyncViewWithFrame(nsIPresContext* aPresContext, const nsString& aPopupAnchor,
                             const nsString& aPopupAlign,
                             nsIFrame* aFrame, PRInt32 aXPos, PRInt32 aYPos);

  NS_IMETHOD KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag);
  NS_IMETHOD ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag);
  
  NS_IMETHOD Escape(PRBool& aHandledFlag);
  NS_IMETHOD Enter();

  nsIMenuFrame* FindMenuWithShortcut(PRUint32 aLetter);

  PRBool IsValidItem(nsIContent* aContent);
  PRBool IsDisabled(nsIContent* aContent);

  NS_IMETHOD KillCloseTimer();

  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      aResult.AssignWithConversion("MenuPopup");
      return NS_OK;
  }


protected:
  // redefine to tell the box system not to move the
  // views.
  virtual void GetLayoutFlags(PRUint32& aFlags);

  // return true if the alignment is horizontal false if vertical
  virtual PRBool GetInitialOrientation(PRBool& aIsHorizontal); 

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

  nsIMenuFrame* mCurrentMenu; // The current menu that is active.
  PRBool mIsCapturingMouseEvents; // Whether or not we're grabbing the mouse events.
  // XXX Hack
  nsIPresContext* mPresContext;  // weak reference

  nsMenuListener* mKeyboardNavigator; // The listener that tells us about key events.
  nsIDOMEventReceiver* mTarget;

  nsIMenuFrame* mTimerMenu; // A menu awaiting closure.
  nsCOMPtr<nsITimer> mCloseTimer; // Close timer.

  PRBool mIsContextMenu;  // is this a context menu?
  
  PRBool mMenuCanOverlapOSBar;    // can we appear over the taskbar/menubar?

}; // class nsMenuPopupFrame

#endif
