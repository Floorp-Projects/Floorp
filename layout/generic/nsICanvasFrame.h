/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsICanvasFrame_h__
#define nsICanvasFrame_h__

#include "nsISupports.h"

// IID for the nsICanvasFrame interface
#define NS_ICANVASFRAME_IID       \
{ 0x9df7db77, 0x49a2, 0x11d5, {0x97, 0x92, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56} }

class nsICanvasFrame : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICANVASFRAME_IID; return iid; }

  /** SetHasFocus tells the CanvasFrame to draw with focus ring
   *  @param aHasFocus PR_TRUE to show focus ring, PR_FALSE to hide it
   */
  NS_IMETHOD SetHasFocus(PRBool aHasFocus) = 0;

};


#endif  // nsICanvasFrame_h__

