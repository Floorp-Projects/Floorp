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
#ifndef nsIFocusTracker_h___
#define nsIFocusTracker_h___

#include "nsISupports.h"
#include "nsIFrame.h"
   

// IID for the nsIFocusTracker interface
#define NS_IFOCUSTRACKER_IID  \
{ 0x81ac51d1, 0x923b, 0x11d2, \
  { 0x91, 0x8f, 0x0, 0x80, 0xc8, 0xe4, 0x4d, 0xb5 } }

class nsIFocusTracker : public nsISupports
{
public:
  /** SetFocus will keep track of the new frame as the focus frame. 
   *  as well as keeping track of the anchor frame; <BR>
   *  @param aFrame will be the focus frame
   *  @param aAnchorFrame will be the anchor frame
   */
  NS_IMETHOD SetFocus(nsIFrame *aFrame, nsIFrame *aAnchorFrame) = 0;

  /** GetFocus give the frame and the anchor frame.
   *  @param aFrame will be the focus frame
   *  @param aAnchorFrame will be the anchor frame
   */
  NS_IMETHOD GetFocus(nsIFrame **aFrame, nsIFrame **aAnchorFrame) = 0;
};


#endif //nsIFocusTracker_h___
