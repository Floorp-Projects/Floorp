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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef nsIRuleWalker_h___
#define nsIRuleWalker_h___

#include "nsCOMPtr.h"
#include "nsIRuleNode.h"

class nsIRuleNode;

// {6ED294B0-2E91-40a6-949D-7A0173691487}
#define NS_IRULEWALKER_IID \
{ 0x6ed294b0, 0x2e91, 0x40a6, { 0x94, 0x9d, 0x7a, 0x1, 0x73, 0x69, 0x14, 0x87 } }

class nsIRuleWalker : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRULEWALKER_IID)

  NS_IMETHOD GetCurrentNode(nsIRuleNode** aResult)=0;
  NS_IMETHOD SetCurrentNode(nsIRuleNode* aNode)=0;

  NS_IMETHOD Forward(nsIStyleRule* aRule)=0;
  NS_IMETHOD Back()=0;

  NS_IMETHOD Reset()=0;

  NS_IMETHOD AtRoot(PRBool* aResult)=0;
};

extern nsresult
NS_NewRuleWalker(nsIRuleNode* aRoot, nsIRuleWalker** aResult);

#endif
