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
#include "nsMenuFrame.h"
#include "nsMenuBarListener.h"
#include "nsIMenuParent.h"
#include "nsIWidget.h"

class nsIContent;

nsIFrame* NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsMenuBarFrame : public nsBoxFrame, public nsIMenuParent
{
public:
  nsMenuBarFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  // nsIMenuParentInterface
  virtual nsMenuFrame* GetCurrentMenuItem();
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem);
  virtual void CurrentMenuIsBeingDestroyed();
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem, PRBool aSelectFirstItem);

  NS_IMETHOD SetActive(PRBool aActiveFlag); 

  virtual PRBool IsMenuBar() { return PR_TRUE; }
  virtual PRBool IsContextMenu() { return PR_FALSE; }
  virtual PRBool IsActive() { return mIsActive; }
  virtual PRBool IsMenu() { return PR_FALSE; }
  virtual PRBool IsOpen() { return PR_TRUE; } // menubars are considered always open

  PRBool IsMenuOpen() { return mCurrentMenu && mCurrentMenu->IsOpen(); }

  // return true if aMenuFrame was the recently closed menu, clearing the
  // the recent menu state in the process.
  PRBool IsRecentlyClosed(nsMenuFrame* aMenuFrame)
  {
    PRBool match = (aMenuFrame == mRecentlyClosedMenu);
    mRecentlyClosedMenu = nsnull;
    return match;
  }
  void SetRecentlyClosed(nsMenuFrame* aRecentlyClosedMenu)
  {
    mRecentlyClosedMenu = aRecentlyClosedMenu;
  }

  void InstallKeyboardNavigator();
  void RemoveKeyboardNavigator();

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void Destroy();

  virtual nsIAtom* GetType() const { return nsGkAtoms::menuBarFrame; }

// Non-interface helpers

  void
  SetStayActive(PRBool aStayActive) { mStayActive = aStayActive; }

  // Called when a menu on the menu bar is clicked on. Returns a menu if one
  // needs to be closed.
  nsMenuFrame* ToggleMenuActiveState();

  // indicate that a menu on the menubar was closed. Returns true if the caller
  // may deselect the menuitem.
  virtual PRBool MenuClosed();

  // Called when Enter is pressed while the menubar is focused. If the current
  // menu is open, let the child handle the key.
  nsMenuFrame* Enter();

  // Used to handle ALT+key combos
  nsMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent);

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return PR_FALSE;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuBar"), aResult);
  }
#endif

protected:
  nsMenuBarListener* mMenuBarListener; // The listener that tells us about key and mouse events.

  // flag that is temporarily set when switching from one menu on the menubar to another
  // to indicate that the menubar should not be deactivated.
  PRPackedBool mStayActive;

  PRPackedBool mIsActive; // Whether or not the menu bar is active (a menu item is highlighted or shown).
  // The current menu that is active (highlighted), which may not be open. This will
  // be null if no menu is active.
  nsMenuFrame* mCurrentMenu;

  // When a menu is closed by clicking the menu label, the menu is rolled up
  // and the mouse event is fired at the menu. The menu that was closed is
  // stored here, to avoid having it reopen again during the mouse event.
  // This is OK to be a weak reference as it is never dereferenced.
  nsMenuFrame* mRecentlyClosedMenu;

  nsIDOMEventTarget* mTarget;

private:
  PRBool mCaretWasVisible;

}; // class nsMenuBarFrame

#endif
