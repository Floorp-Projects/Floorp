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



// IID for the nsICaret interface
#define NS_ICARET_IID       \
{ 0xa6cf90e1, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsCaretProperties;

class nsICaret: public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICARET_IID; return iid; }

  NS_IMETHOD Init(nsIPresShell *inPresShell, nsCaretProperties *inCaretProperties) = 0;
  
  /** SetCaretVisible will set the visibility of the caret
   *  @param inMakeVisible PR_TRUE to show the caret, PR_FALSE to hide it
   */
  NS_IMETHOD SetCaretVisible(PRBool inMakeVisible) = 0;

  /** SetCaretReadOnly set the appearance of the caret
   *  @param inMakeReadonly PR_TRUE to show the caret in a 'read only' state,
   *  PR_FALSE to show the caret in normal, editing state
   */
  NS_IMETHOD SetCaretReadOnly(PRBool inMakeReadonly) = 0;

  /** Refresh
   *  Refresh the caret after the frame it is being drawn in has painted
   */
 	NS_IMETHOD Refresh(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect) = 0;

  /** ClearFrameRefs
   *  The caret stores a reference to the frame that the caret was last drawn in.
   *  This is called to tell the caret that that frame is going away.
   */
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

};

extern nsresult NS_NewCaret(nsICaret** aInstancePtrResult);

