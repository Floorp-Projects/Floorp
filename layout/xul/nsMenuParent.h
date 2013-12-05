/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuParent_h___
#define nsMenuParent_h___

class nsMenuFrame;

/*
 * nsMenuParent is an interface implemented by nsMenuBarFrame and nsMenuPopupFrame
 * as both serve as parent frames to nsMenuFrame.
 *
 * Don't implement this interface on other classes unless you also fix up references,
 * as this interface is directly cast to and from nsMenuBarFrame and nsMenuPopupFrame.
 */

class nsMenuParent {

public:
  // returns the menu frame of the currently active item within the menu
  virtual nsMenuFrame *GetCurrentMenuItem() = 0;
  // sets the currently active menu frame.
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem) = 0;
  // indicate that the current menu frame is being destroyed, so clear the
  // current menu item
  virtual void CurrentMenuIsBeingDestroyed() = 0;
  // deselects the current item and closes its popup if any, then selects the
  // new item aMenuItem. For a menubar, if another menu is already open, the
  // new menu aMenuItem is opened. In this case, if aSelectFirstItem is true,
  // select the first item in it. For menupopups, the menu is not opened and
  // the aSelectFirstItem argument is not used.
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem, bool aSelectFirstItem) = 0;

  // returns true if the menupopup is open. For menubars, returns false.
  virtual bool IsOpen() = 0;
  // returns true if the menubar is currently active. For menupopups, returns false.
  virtual bool IsActive() = 0;
  // returns true if this is a menubar. If false, it is a popup
  virtual bool IsMenuBar() = 0;
  // returns true if this is a menu, which has a tag of menupopup or popup.
  // Otherwise, this returns false
  virtual bool IsMenu() = 0;
  // returns true if this is a context menu
  virtual bool IsContextMenu() = 0;

  // indicate that the menubar should become active or inactive
  NS_IMETHOD SetActive(bool aActiveFlag) = 0;

  // notify that the menu has been closed and any active state should be
  // cleared. This should return true if the menu should be deselected
  // by the caller.
  virtual bool MenuClosed() = 0;

  // Lock this menu and its parents until they're closed or unlocked.
  // A menu being "locked" means that all events inside it that would change the
  // selected menu item should be ignored.
  // This is used when closing the popup is delayed because of a blink or fade
  // animation.
  virtual void LockMenuUntilClosed(bool aLock) = 0;
  virtual bool IsMenuLocked() = 0;
};

#endif

