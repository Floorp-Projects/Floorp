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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIPopupSetFrame_h___
#define nsIPopupSetFrame_h___

// {E2D804A1-50CA-11d3-BF87-00105A1B0627}
#define NS_IPOPUPSETFRAME_IID \
{ 0xe2d804a1, 0x50ca, 0x11d3, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIFrame;
class nsIContent;
class nsIDOMElement;

#include "nsString.h"

class nsIPopupSetFrame : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPOPUPSETFRAME_IID; return iid; }

  NS_IMETHOD ShowPopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                       PRInt32 aXPos, PRInt32 aYPos, 
                       const nsString& aPopupType, const nsString& anAnchorAlignment,
                       const nsString& aPopupAlignment) = 0;
  NS_IMETHOD HidePopup(nsIFrame* aPopup) = 0;
  NS_IMETHOD DestroyPopup(nsIFrame* aPopup) = 0;

  NS_IMETHOD AddPopupFrame(nsIFrame* aPopup) = 0;
  NS_IMETHOD RemovePopupFrame(nsIFrame* aPopup) = 0;
};

#endif

