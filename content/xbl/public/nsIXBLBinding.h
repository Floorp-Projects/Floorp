/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  Private interface to the XBL Binding

*/

#ifndef nsIXBLBinding_h__
#define nsIXBLBinding_h__

#include "nsString.h"
#include "nsISupports.h"

class nsIContent;
class nsISupportsArray;
class nsIScriptContext;

// {DDDBAD20-C8DF-11d3-97FB-00400553EEF0}
#define NS_IXBLBINDING_IID \
{ 0xdddbad20, 0xc8df, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLBinding : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLBINDING_IID; return iid; }

  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult) = 0;
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding) = 0;

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent) = 0;
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent) = 0;

  NS_IMETHOD GetBindingElement(nsIContent** aResult) = 0;
  NS_IMETHOD SetBindingElement(nsIContent* aElement) = 0;

  NS_IMETHOD GetInsertionPoint(nsIContent** aResult) = 0;
  
  NS_IMETHOD GenerateAnonymousContent(nsIContent* aBoundElement) = 0;
  NS_IMETHOD InstallEventHandlers(nsIContent* aBoundElement) = 0;
  NS_IMETHOD InstallProperties(nsIContent* aBoundElement) = 0;

  NS_IMETHOD GetBaseTag(nsIAtom** aResult) = 0;

  // Called when an attribute changes on a binding.
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag) = 0;

  NS_IMETHOD RemoveScriptReferences(nsIScriptContext* aContext) = 0;
};

extern nsresult
NS_NewXBLBinding(nsIXBLBinding** aResult);

#endif // nsIXBLBinding_h__
