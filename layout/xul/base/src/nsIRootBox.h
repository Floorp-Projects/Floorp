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
 *   David Hyatt (hyatt@netscape.com)
 */


#ifndef nsIRootBox_h___
#define nsIRootBox_h___

#include "nsISupports.h"
class nsIFrame;

// {DF05F6AB-320B-4e06-AFB3-E39E632A7555}
#define NS_IROOTBOX_IID \
{ 0xdf05f6ab, 0x320b, 0x4e06, { 0xaf, 0xb3, 0xe3, 0x9e, 0x63, 0x2a, 0x75, 0x55 } }

class nsIRootBox : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IROOTBOX_IID; return iid; }

  NS_IMETHOD GetPopupSetFrame(nsIFrame** aResult)=0;
  NS_IMETHOD SetPopupSetFrame(nsIFrame* aPopupSet)=0;
};

#endif

