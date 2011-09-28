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

#ifndef nsIListControlFrame_h___
#define nsIListControlFrame_h___

#include "nsQueryFrame.h"
#include "nsFont.h"

class nsAString;
class nsIContent;

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
  virtual void GetOptionText(PRInt32 aIndex, nsAString & aStr) = 0;

  /**
   * Get the Selected Item's index
   *
   */
  virtual PRInt32 GetSelectedIndex() = 0;

  /**
   * Return current option. The current option is the option displaying
   * the focus ring when the listbox is focused.
   */
  virtual already_AddRefed<nsIContent> GetCurrentOption() = 0;

  /**
   * Initiates mouse capture for the listbox
   *
   */
  virtual void CaptureMouseEvents(PRBool aGrabMouseEvents) = 0;

  /**
   * Returns the height of a single row in the list.  This is the
   * maximum of the heights of all the options/optgroups.
   */
  virtual nscoord GetHeightOfARow() = 0;

  /**
   * Returns the number of options in the listbox
   */

  virtual PRInt32 GetNumberOfOptions() = 0; 

  /**
   * 
   */
  virtual void SyncViewWithFrame() = 0;

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
  virtual void ComboboxFinish(PRInt32 aIndex) = 0;

  /**
   * Notification that the content has been reset
   */
  virtual void OnContentReset() = 0;
};

#endif

