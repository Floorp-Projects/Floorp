/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsISelectControlFrame_h___
#define nsISelectControlFrame_h___

#include "nsISupports.h"

// IID for the nsISelectControlFrame class
#define NS_ISELECTCONTROLFRAME_IID    \
{ 0x162a2ae3, 0x5a79, 0x11d3,  \
  { 0x96, 0xea, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }
// {162A2AE3-5A79-11d3-96EA-0060B0FB9956}

/** 
  * nsISelectControlFrame is the interface for combo boxes and listboxes
  */
class nsISelectControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISELECTCONTROLFRAME_IID)

  /**
   * Adds an option to the list at index
   */

  NS_IMETHOD AddOption(nsIPresContext* aPresContext, PRInt32 index) = 0;

  /**
   * Removes the option at index
   */

  NS_IMETHOD RemoveOption(nsIPresContext* aPresContext, PRInt32 index) = 0; 

  /**
   * Sets the select state of the option at index
   */
  NS_IMETHOD GetOptionSelected(PRInt32 index, PRBool* value) = 0;

  /**
   * Sets the select state of the option at index
   */
  NS_IMETHOD DoneAddingContent(PRBool aIsDone) = 0;

  /**
   * Notify the frame when an option is selected
   */
  NS_IMETHOD OnOptionSelected(nsIPresContext* aPresContext,
                              PRInt32 aIndex,
                              PRBool aSelected) = 0;

  /**
   * For the content model to tell if there's a dummy frame or not
   */
  NS_IMETHOD GetDummyFrame(nsIFrame** aFrame) = 0;
  NS_IMETHOD SetDummyFrame(nsIFrame* aFrame) = 0;
};

#endif
