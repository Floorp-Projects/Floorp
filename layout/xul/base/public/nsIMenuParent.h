/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIMenuParent_h___
#define nsIMenuParent_h___


// {D407BF61-3EFA-11d3-97FA-00400553EEF0}
#define NS_IMENUPARENT_IID \
{ 0xd407bf61, 0x3efa, 0x11d3, { 0x97, 0xfa, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }


class nsIMenuParent : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMENUPARENT_IID; return iid; }

  NS_IMETHOD SetCurrentMenuItem(nsIFrame* aMenuItem) = 0;
  NS_IMETHOD GetNextMenuItem(nsIFrame* aStart, nsIFrame** aResult) = 0;
  NS_IMETHOD GetPreviousMenuItem(nsIFrame* aStart, nsIFrame** aResult) = 0;

  NS_IMETHOD SetActive(PRBool aActiveFlag) = 0;
  NS_IMETHOD GetIsActive(PRBool& isActive) = 0;
  
  NS_IMETHOD IsMenuBar(PRBool& isMenuBar) = 0;

  NS_IMETHOD DismissChain() = 0;
  NS_IMETHOD HideChain() = 0;
};

#endif

