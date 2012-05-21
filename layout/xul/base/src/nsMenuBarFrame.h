/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsMenuParent.h"

class nsIContent;

nsIFrame* NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsMenuBarFrame : public nsBoxFrame, public nsMenuParent
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsMenuBarFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsMenuBarFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  // nsMenuParent interface
  virtual nsMenuFrame* GetCurrentMenuItem();
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem);
  virtual void CurrentMenuIsBeingDestroyed();
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem, bool aSelectFirstItem);

  NS_IMETHOD SetActive(bool aActiveFlag); 

  virtual bool IsMenuBar() { return true; }
  virtual bool IsContextMenu() { return false; }
  virtual bool IsActive() { return mIsActive; }
  virtual bool IsMenu() { return false; }
  virtual bool IsOpen() { return true; } // menubars are considered always open

  bool IsMenuOpen() { return mCurrentMenu && mCurrentMenu->IsOpen(); }

  void InstallKeyboardNavigator();
  void RemoveKeyboardNavigator();

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIAtom* GetType() const { return nsGkAtoms::menuBarFrame; }

  virtual void LockMenuUntilClosed(bool aLock) {}
  virtual bool IsMenuLocked() { return false; }

// Non-interface helpers

  void
  SetStayActive(bool aStayActive) { mStayActive = aStayActive; }

  // Called when a menu on the menu bar is clicked on. Returns a menu if one
  // needs to be closed.
  nsMenuFrame* ToggleMenuActiveState();

  bool IsActiveByKeyboard() { return mActiveByKeyboard; }
  void SetActiveByKeyboard() { mActiveByKeyboard = true; }

  // indicate that a menu on the menubar was closed. Returns true if the caller
  // may deselect the menuitem.
  virtual bool MenuClosed();

  // Called when Enter is pressed while the menubar is focused. If the current
  // menu is open, let the child handle the key.
  nsMenuFrame* Enter(nsGUIEvent* aEvent);

  // Used to handle ALT+key combos
  nsMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent);

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
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
  bool mStayActive;

  bool mIsActive; // Whether or not the menu bar is active (a menu item is highlighted or shown).

  // whether the menubar was made active via the keyboard.
  bool mActiveByKeyboard;

  // The current menu that is active (highlighted), which may not be open. This will
  // be null if no menu is active.
  nsMenuFrame* mCurrentMenu;

  nsIDOMEventTarget* mTarget;

}; // class nsMenuBarFrame

#endif
