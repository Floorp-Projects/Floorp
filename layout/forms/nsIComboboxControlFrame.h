/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIComboboxControlFrame_h___
#define nsIComboboxControlFrame_h___

#include "nsQueryFrame.h"
#include "nsFont.h"

class nsString;
class nsIContent;
class nsCSSFrameConstructor;

/** 
  * nsIComboboxControlFrame is the common interface for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsIComboboxControlFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIComboboxControlFrame)

  /**
   * Indicates whether the list is dropped down
   */
  virtual bool IsDroppedDown() = 0;

  /**
   * Shows or hides the drop down
   */
  virtual void ShowDropDown(bool aDoDropDown) = 0;

  /**
   * Gets the Drop Down List
   */
  virtual nsIFrame* GetDropDown() = 0;

  /**
   * Sets the Drop Down List
   */
  virtual void SetDropDown(nsIFrame* aDropDownFrame) = 0;

  /**
   * Tells the combobox to roll up
   */
  virtual void RollupFromList() = 0;

  /**
   * Redisplay the selected text (will do nothing if text has not changed).
   * This method might destroy this frame or any others that happen to be
   * around.  It might even run script.
   */
  NS_IMETHOD RedisplaySelectedText() = 0;

  /**
   * Method for the listbox to set and get the recent index
   */
  virtual PRInt32 UpdateRecentIndex(PRInt32 aIndex) = 0;

  /**
   * Notification that the content has been reset
   */
  virtual void OnContentReset() = 0;
  
  /**
   * This returns the index of the item that is currently being displayed
   * in the display area. It may differ from what the currently Selected index
   * is in in the dropdown.
   *
   * Detailed explanation: 
   * When the dropdown is dropped down via a mouse click and the user moves the mouse 
   * up and down without clicking, the currently selected item is being tracking inside 
   * the dropdown, but the combobox is not being updated. When the user selects items
   * with the arrow keys, the combobox is being updated. So when the user clicks outside
   * the dropdown and it needs to roll up it has to decide whether to keep the current 
   * selection or not. This method is used to get the current index in the combobox to
   * compare it to the current index in the dropdown to see if the combox has been updated
   * and that way it knows whether to "cancel" the current selection residing in the 
   * dropdown. Or whether to leave the selection alone.
   */
  virtual PRInt32 GetIndexOfDisplayArea() = 0;
};

#endif

