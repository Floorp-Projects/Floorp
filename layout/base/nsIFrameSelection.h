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
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE!!  This is not a general class, but specific to layout and frames.
 * Consumers looking for the general selection interface should look at
 * nsIDOMSelection.
 */

#ifndef nsIFrameSelection_h___
#define nsIFrameSelection_h___

#include "nsISupports.h"
#include "nsIFrame.h"
#include "nsIFocusTracker.h"   

// IID for the nsIFrameSelection interface
#define NS_IFRAMESELECTION_IID      \
{ 0xf46e4171, 0xdeaa, 0x11d1, \
  { 0x97, 0xfc, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


//----------------------------------------------------------------------

// Selection interface
class nsIFrameSelection : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFRAMESELECTION_IID; return iid; }

  /** Init will initialize the frame selector with the necessary focus tracker to 
   *  be used by most of the methods
   *  @param aTracker is the parameter to be used for most of the other calls for callbacks ect
   */
  NS_IMETHOD Init(nsIFocusTracker *aTracker) = 0;

  /** ShutDown will be called when the owner of the frame selection is shutting down
   *  this should be the time to release all member variable interfaces. all methods
   *  called after ShutDown should return NS_ERROR_FAILURE
   */
  NS_IMETHOD ShutDown() = 0;

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGuiEvent) = 0;

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleKeyEvent(nsGUIEvent *aGuiEvent) = 0;

  /** TakeFocus will take the focus to the new frame at the new offset and 
   *  will either extend the selection from the old anchor, or replace the old anchor.
   *  the old anchor and focus position may also be used to deselect things
   *  @param aNewfocus is the content that wants the focus
   *  @param aContentOffset is the content offset of the parent aNewFocus
   *  @param aContinueSelection is the flag that tells the selection to keep the old anchor point or not.
   */
  NS_IMETHOD TakeFocus(nsIContent *aNewFocus, PRUint32 aContentOffset, PRBool aContinueSelection) = 0;

  /** EnableFrameNotification
   *  mutch like start batching, except all dirty calls are ignored. no notifications will go 
   *  out until enableNotifications with a PR_TRUE is called
   */
  NS_IMETHOD EnableFrameNotification(PRBool aEnable) = 0;

  /** Lookup Selection
   *  returns in frame coordinates the selection beginning and ending with the type of selection given
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aStart start return frame indexed value
   * @param aEnd   end return frame indexed value
   * @param aDrawSelected return value if this offset,length has anything in the selected area
   * @param aFlag what type of selection not used now
   */
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             PRInt32 *aStart, PRInt32 *aEnd, PRBool *aDrawSelected, PRUint32 aFlag/*not used*/) = 0;
};


#endif /* nsIFrameSelection_h___ */
