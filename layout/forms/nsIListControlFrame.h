/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIListControlFrame_h___
#define nsIListControlFrame_h___

#include "nsQueryFrame.h"
#include "nsFont.h"

class nsAString;
class nsIContent;

namespace mozilla {
namespace dom {
class HTMLOptionElement;
} // namespace dom
} // namespace mozilla

/** 
  * nsIListControlFrame is the interface for frame-based listboxes.
  */
class nsIListControlFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIListControlFrame)

  /**
   * Sets the ComboBoxFrame
   *
   */
  virtual void SetComboboxFrame(nsIFrame* aComboboxFrame) = 0;

  /**
   * Get the display string for an item
   */
  virtual void GetOptionText(uint32_t aIndex, nsAString& aStr) = 0;

  /**
   * Get the Selected Item's index
   *
   */
  virtual int32_t GetSelectedIndex() = 0;

  /**
   * Return current option. The current option is the option displaying
   * the focus ring when the listbox is focused.
   */
  virtual mozilla::dom::HTMLOptionElement* GetCurrentOption() = 0;

  /**
   * Initiates mouse capture for the listbox
   *
   */
  virtual void CaptureMouseEvents(bool aGrabMouseEvents) = 0;

  /**
   * Returns the height of a single row in the list.  This is the
   * maximum of the heights of all the options/optgroups.
   */
  virtual nscoord GetHeightOfARow() = 0;

  /**
   * Returns the number of options in the listbox
   */

  virtual uint32_t GetNumberOfOptions() = 0;

  /**
   * Called by combobox when it's about to drop down
   */
  virtual void AboutToDropDown() = 0;

  /**
   * Called by combobox when it's about to roll up
   */
  virtual void AboutToRollup() = 0;

  /**
   * Fire on change (used by combobox)
   */
  virtual void FireOnChange() = 0;

  /**
   * Tell the selected list to roll up and ensure that the proper index is
   * selected, possibly firing onChange if the index has changed
   *
   * @param aIndex the index to actually select
   */
  virtual void ComboboxFinish(int32_t aIndex) = 0;

  /**
   * Notification that the content has been reset
   */
  virtual void OnContentReset() = 0;
};

#endif

