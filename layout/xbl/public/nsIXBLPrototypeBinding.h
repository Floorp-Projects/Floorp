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

#ifndef nsIXBLPrototypeBinding_h__
#define nsIXBLPrototypeBinding_h__

#include "nsString.h"

class nsIContent;
class nsIDocument;
class nsIXBLDocumentInfo;
class nsIXBLPrototypeHandler;

// {34D700F5-C1A2-4408-A0B1-DD8F891DD1FE}
#define NS_IXBLPROTOTYPEBINDING_IID \
{ 0x34d700f5, 0xc1a2, 0x4408, { 0xa0, 0xb1, 0xdd, 0x8f, 0x89, 0x1d, 0xd1, 0xfe } }

class nsIXBLPrototypeBinding : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLPROTOTYPEBINDING_IID; return iid; }

  NS_IMETHOD GetBindingElement(nsIContent** aResult)=0;
  NS_IMETHOD SetBindingElement(nsIContent* aElement)=0;

  NS_IMETHOD GetBindingURI(nsCString& aResult)=0;
  NS_IMETHOD GetDocURI(nsCString& aResult)=0;
  NS_IMETHOD GetID(nsCString& aResult)=0;

  NS_IMETHOD GetAllowScripts(PRBool* aResult)=0;

  NS_IMETHOD InheritsStyle(PRBool* aResult)=0;

  NS_IMETHOD GetPrototypeHandler(nsIXBLPrototypeHandler** aHandler)=0;
  NS_IMETHOD SetPrototypeHandler(nsIXBLPrototypeHandler* aHandler)=0;
  
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag, 
                              nsIContent* aChangedElement, nsIContent* aAnonymousContent)=0;

  NS_IMETHOD SetBasePrototype(nsIXBLPrototypeBinding* aBinding)=0;
  NS_IMETHOD GetBasePrototype(nsIXBLPrototypeBinding** aResult)=0;

  NS_IMETHOD HasBasePrototype(PRBool* aResult)=0;
  NS_IMETHOD SetHasBasePrototype(PRBool aHasBase)=0;

  NS_IMETHOD GetXBLDocumentInfo(nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)=0;

  NS_IMETHOD SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)=0;

  NS_IMETHOD HasInsertionPoints(PRBool* aResult)=0;

  NS_IMETHOD GetInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                               nsIContent* aChild, nsIContent** aResult)=0;

  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                                     nsIContent** aResult, PRBool* aMultipleInsertionPoints)=0;

  NS_IMETHOD GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aTag)=0;
  NS_IMETHOD SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag)=0;
};

extern nsresult
NS_NewXBLPrototypeBinding(const nsCString& aRef, 
                          nsIContent* aElement, nsIXBLDocumentInfo* aInfo, 
                          nsIXBLPrototypeBinding** aResult);

#endif // nsIXBLPrototypeBinding_h__
