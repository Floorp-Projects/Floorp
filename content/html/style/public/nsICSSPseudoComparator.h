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
#ifndef nsICSSPseudoComparator_h___
#define nsICSSPseudoComparator_h___

// {4B122120-0F2D-4e88-AFE9-84A9AE2404E5}
#define NS_ICSS_PSEUDO_COMPARATOR_IID     \
{ 0x4b122120, 0xf2d, 0x4e88, { 0xaf, 0xe9, 0x84, 0xa9, 0xae, 0x24, 0x4, 0xe5 } }

class nsIAtom;
struct nsCSSSelector;

class nsICSSPseudoComparator: public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_PSEUDO_COMPARATOR_IID; return iid; }

  NS_IMETHOD  PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)=0;
};

#endif /* nsICSSPseudoComparator_h___ */
