/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * nsIDOMSelection: the preferred interface into the nsRangeList selection.
 *
 * This interface is meant to be IDL'ed and XPCOMified eventually.
 */
#ifndef nsIDOMSelection_h___
#define nsIDOMSelection_h___

#include "nsISupports.h"

// IID for the nsIDOMSelection interface
#define NS_IDOMSELECTION_IID      \
{ 0xa6cf90e1, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

//----------------------------------------------------------------------

class nsIDOMRange;
class nsIDOMSelectionListener;

// Selection interface
class nsIDOMSelection : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMSELECTION_IID; return iid; }

  /*
   * Get the node and offset for the anchor point
   */
  NS_IMETHOD GetAnchorNodeAndOffset(nsIDOMNode** outAnchorNode, PRInt32 *outAnchorOffset) = 0;

  /*
   * Get the node and offset for the focus point
   */
  NS_IMETHOD GetFocusNodeAndOffset(nsIDOMNode** outFocusNode, PRInt32 *outFocusOffset) = 0;

  /*
   * ClearSelection zeroes the selection
   */
  NS_IMETHOD ClearSelection() = 0;

  /*
   * Collapse sets the whole selection to be one point.
   */
  NS_IMETHOD Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset) = 0;

  /*
   * IsCollapsed -- is the whole selection just one point, or unset?
   */
  NS_IMETHOD IsCollapsed(PRBool* aIsCollapsed) = 0;

  /*
   * Extend extends the selection away from the anchor.
   */
  NS_IMETHOD Extend(nsIDOMNode* aParentNode, PRInt32 aOffset) = 0;

  /** AddRange adds the specified range to the selection
   *  @param aRange is the range to be added
   */
  NS_IMETHOD AddRange(nsIDOMRange* aRange) = 0;

  /** DeleteFromDocument
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   */
  NS_IMETHOD DeleteFromDocument() = 0;

  NS_IMETHOD AddSelectionListener(nsIDOMSelectionListener* inNewListener) = 0;
  NS_IMETHOD RemoveSelectionListener(nsIDOMSelectionListener* inListenerToRemove) = 0;

};


#endif /* nsISelection_h___ */
