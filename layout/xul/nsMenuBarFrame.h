/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsMenuBarFrame
//

#ifndef nsMenuBarFrame_h__
#define nsMenuBarFrame_h__

#include "mozilla/Attributes.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsMenuFrame.h"
#include "nsMenuBarListener.h"
#include "nsMenuParent.h"

class nsIContent;

nsIFrame* NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsMenuBarFrame final : public nsBoxFrame, public nsMenuParent
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMenuBarFrame)

  explicit nsMenuBarFrame(nsStyleContext* aContext);

  // nsMenuParent interface
  virtual nsMenuFrame* GetCurrentMenuItem() override;
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem) override;
  virtual void CurrentMenuIsBeingDestroyed() override;
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem,
                            bool aSelectFirstItem,
                            bool aFromKey) override;

  NS_IMETHOD SetActive(bool aActiveFlag) override;

  virtual bool IsMenuBar() override { return true; }
  virtual bool IsContextMenu() override { return false; }
  virtual bool IsActive() override { return mIsActive; }
  virtual bool IsMenu() override { return false; }
  virtual bool IsOpen() override { return true; } // menubars are considered always open

  bool IsMenuOpen() { return mCurrentMenu && mCurrentMenu->IsOpen(); }

  void InstallKeyboardNavigator();
  void RemoveKeyboardNavigator();

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;

  virtual void LockMenuUntilClosed(bool aLock) override {}
  virtual bool IsMenuLocked() override { return false; }

// Non-interface helpers

  // The 'stay active' flag is set when navigating from one top-level menu
  // to another, to prevent the menubar from deactivating and submenus from
  // firing extra DOMMenuItemActive events.
  bool GetStayActive() { return mStayActive; }
  void SetStayActive(bool aStayActive) { mStayActive = aStayActive; }

  // Called when a menu on the menu bar is clicked on. Returns a menu if one
  // needs to be closed.
  nsMenuFrame* ToggleMenuActiveState();

  bool IsActiveByKeyboard() { return mActiveByKeyboard; }
  void SetActiveByKeyboard() { mActiveByKeyboard = true; }

  // indicate that a menu on the menubar was closed. Returns true if the caller
  // may deselect the menuitem.
  virtual bool MenuClosed() override;

  // Called when Enter is pressed while the menubar is focused. If the current
  // menu is open, let the child handle the key.
  nsMenuFrame* Enter(mozilla::WidgetGUIEvent* aEvent);

  // Used to handle ALT+key combos
  nsMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, bool aPeek);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuBar"), aResult);
  }
#endif

protected:
  RefPtr<nsMenuBarListener> mMenuBarListener; // The listener that tells us about key and mouse events.

  // flag that is temporarily set when switching from one menu on the menubar to another
  // to indicate that the menubar should not be deactivated.
  bool mStayActive;

  bool mIsActive; // Whether or not the menu bar is active (a menu item is highlighted or shown).

  // whether the menubar was made active via the keyboard.
  bool mActiveByKeyboard;

  // The current menu that is active (highlighted), which may not be open. This will
  // be null if no menu is active.
  nsMenuFrame* mCurrentMenu;
}; // class nsMenuBarFrame

#endif
