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

/* NOTE This should be renamed asap to nsIFrameSelection
 * -- it's not a general class, but specific to frames.
 * nsIDOMSelection is the more general selection interface.
 */

#ifndef nsISelection_h___
#define nsISelection_h___

#include "nsISupports.h"
#include "nsIFrame.h"
#include "nsIFocusTracker.h"   

// IID for the nsISelection interface
#define NS_ISELECTION_IID      \
{ 0xf46e4171, 0xdeaa, 0x11d1, \
  { 0x97, 0xfc, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

//----------------------------------------------------------------------

// Selection interface
class nsISelection : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_ISELECTION_IID; return iid; }

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param tracker to ask where the current focus is and to set the new anchor ect.
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleKeyEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent, nsIFrame *aFrame) = 0;

  /** TakeFocus will take the focus to the new frame at the new offset and 
   *  will either extend the selection from the old anchor, or replace the old anchor.
   *  the old anchor and focus position may also be used to deselect things
   *  @param aTracker  we need a focus tracker to get the old focus ect.
   *  @param aFrame is the frame that wants the focus
   *  @param aOffset is the offset in the aFrame that will get the focus point
   *  @param aContentOffset is the offset in the node of the aFrame that is reflected be aOffset
   *  @param aContinueSelection is the flag that tells the selection to keep the old anchor point or not.
   */
  NS_IMETHOD TakeFocus(nsIFocusTracker *aTracker, nsIFrame *aFrame, PRInt32 aOffset, PRInt32 aContentOffset, PRBool aContinueSelection) = 0;

  /** ResetSelection will top down search for frames that need selection
   */
  NS_IMETHOD ResetSelection(nsIFocusTracker *aTracker, nsIFrame *aStartFrame) = 0;

};


#endif /* nsISelection_h___ */
