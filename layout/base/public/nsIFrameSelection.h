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

/** nsSelectionStruct is used to pass structured parameters to the SelectionCalls.
 */
struct nsSelectionStruct
{
  enum {SELON = 1,SELALL = 2,SELTOEND = 4,SELTOBEGIN = 8,CHECKANCHOR=16,CHECKFOCUS=32};//this is not a bit flag
  PRUint32 mType;
  PRUint32 mStartContent;//in content offsets.
  PRUint32 mEndContent;
  PRUint32 mStartFrame;
  PRUint32 mEndFrame;
  PRUint32 mAnchorOffset;
  PRUint32 mFocusOffset;
  nsDirection mDir; //tells you if you have to swap the begin and end points.
  PRBool   mForceRedraw;
};


//----------------------------------------------------------------------

// Selection interface
class nsIFrameSelection : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFRAMESELECTION_IID; return iid; }

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param tracker to ask where the current focus is and to set the new anchor ect.
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleTextEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent) = 0;

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param tracker to ask where the current focus is and to set the new anchor ect.
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleKeyEvent(nsIFocusTracker *aTracker, nsGUIEvent *aGuiEvent) = 0;

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

  /** EnableFrameNotification
   *  mutch like start batching, except all dirty calls are ignored. no notifications will go 
   *  out until enableNotifications with a PR_TRUE is called
   */
  NS_IMETHOD EnableFrameNotification(PRBool aEnable) = 0;
};


#endif /* nsIFrameSelection_h___ */
