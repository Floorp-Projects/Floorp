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
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Dan Rosen <dr@netscape.com>
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
// nsMenuBarFrame
//

#ifndef nsMenuBarFrame_h__
#define nsMenuBarFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsMenuBarListener.h"
#include "nsMenuListener.h"
#include "nsIMenuParent.h"
#include "nsIWidget.h"

class nsIContent;
class nsIMenuFrame;

nsresult NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;

class nsMenuBarFrame : public nsBoxFrame, public nsIMenuParent
{
public:
  nsMenuBarFrame(nsIPresShell* aShell);
  virtual ~nsMenuBarFrame();

  NS_DECL_ISUPPORTS

  // nsIMenuParentInterface
  NS_IMETHOD GetCurrentMenuItem(nsIMenuFrame** aResult);
  NS_IMETHOD SetCurrentMenuItem(nsIMenuFrame* aMenuItem);
  NS_IMETHOD GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult);
  NS_IMETHOD SetActive(PRBool aActiveFlag); 
  NS_IMETHOD GetIsActive(PRBool& isActive) { isActive = IsActive(); return NS_OK; };
  NS_IMETHOD IsMenuBar(PRBool& isMenuBar) { isMenuBar = PR_TRUE; return NS_OK; };
  NS_IMETHOD ConsumeOutsideClicks(PRBool& aConsumeOutsideClicks) \
    {aConsumeOutsideClicks = PR_FALSE; return NS_OK;};
  NS_IMETHOD ClearRecentlyRolledUp();
  NS_IMETHOD RecentlyRolledUp(nsIMenuFrame *aMenuFrame, PRBool *aJustRolledUp);

  NS_IMETHOD SetIsContextMenu(PRBool aIsContextMenu) { return NS_OK; };
  NS_IMETHOD GetIsContextMenu(PRBool& aIsContextMenu) { aIsContextMenu = PR_FALSE; return NS_OK; }; 
  
  NS_IMETHOD IsActive() { return mIsActive; };

  NS_IMETHOD IsOpen();
  NS_IMETHOD KillPendingTimers();

  // Closes up the chain of open cascaded menus.
  NS_IMETHOD DismissChain();

  // Hides the chain of cascaded menus without closing them up.
  NS_IMETHOD HideChain();

  NS_IMETHOD InstallKeyboardNavigator();
  NS_IMETHOD RemoveKeyboardNavigator();

  NS_IMETHOD GetWidget(nsIWidget **aWidget);
  // The dismissal listener gets created and attached to the window.
  NS_IMETHOD CreateDismissalListener();

  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

// Non-interface helpers

  // Called when a menu on the menu bar is clicked on.
  void ToggleMenuActiveState();
  
  // Used to move up, down, left, and right in menus.
  NS_IMETHOD KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag);
  NS_IMETHOD ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag);
  // Called when the ESC key is held down to close levels of menus.
  NS_IMETHOD Escape(PRBool& aHandledFlag);
  // Called to execute a menu item.
  NS_IMETHOD Enter();

  // Used to handle ALT+key combos
  nsIMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent);

  PRBool IsValidItem(nsIContent* aContent);
  PRBool IsDisabled(nsIContent* aContent);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuBar"), aResult);
  }
#endif

protected:
  nsMenuBarListener* mMenuBarListener; // The listener that tells us about key and mouse events.
  nsMenuListener* mKeyboardNavigator;

  PRBool mIsActive; // Whether or not the menu bar is active (a menu item is highlighted or shown).
  nsIMenuFrame* mCurrentMenu; // The current menu that is active.

  // Can contain a menu that was rolled up via nsIMenuDismissalListener::Rollup()
  // if nothing has happened since the last click. Otherwise, contains nsnull.
  nsIMenuFrame* mRecentRollupMenu; 

  nsIDOMEventReceiver* mTarget;

  // XXX Hack
  nsPresContext* mPresContext;  // weak reference

private:
  PRBool mCaretWasVisible;

}; // class nsMenuBarFrame

#endif
