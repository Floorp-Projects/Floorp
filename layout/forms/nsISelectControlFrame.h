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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#ifndef nsISelectControlFrame_h___
#define nsISelectControlFrame_h___

#include "nsQueryFrame.h"

class nsIDOMHTMLOptionElement;

/** 
  * nsISelectControlFrame is the interface for combo boxes and listboxes
  */
class nsISelectControlFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsISelectControlFrame)

  /**
   * Adds an option to the list at index
   */

  NS_IMETHOD AddOption(PRInt32 index) = 0;

  /**
   * Removes the option at index.  The caller must have a live script
   * blocker while calling this method.
   */
  NS_IMETHOD RemoveOption(PRInt32 index) = 0; 

  /**
   * Sets whether the parser is done adding children
   * @param aIsDone whether the parser is done adding children
   */
  NS_IMETHOD DoneAddingChildren(bool aIsDone) = 0;

  /**
   * Notify the frame when an option is selected
   */
  NS_IMETHOD OnOptionSelected(PRInt32 aIndex, bool aSelected) = 0;

  /**
   * Notify the frame when selectedIndex was changed.  This might
   * destroy the frame.
   */
  NS_IMETHOD OnSetSelectedIndex(PRInt32 aOldIndex, PRInt32 aNewIndex) = 0;

};

#endif
