/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIXBLPrototypeBinding_h__
#define nsIXBLPrototypeBinding_h__

#include "nsString.h"

class nsIContent;
class nsIDocument;
class nsIDOMEventReceiver;
class nsIXBLDocumentInfo;
class nsIXBLPrototypeHandler;
class nsIXBLPrototypeProperty; 
class nsIXBLBinding;
class nsISupportsArray;
class nsCString; 
class nsIScriptContext;

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

  NS_IMETHOD BindingAttached(nsIDOMEventReceiver* aReceiver)=0;
  NS_IMETHOD BindingDetached(nsIDOMEventReceiver* aReceiver)=0;

  NS_IMETHOD LoadResources(PRBool* aLoaded)=0;
  NS_IMETHOD AddResource(nsIAtom* aResourceType, const nsAReadableString& aSrc)=0;

  NS_IMETHOD InheritsStyle(PRBool* aResult)=0;

  NS_IMETHOD GetPrototypeHandlers(nsIXBLPrototypeHandler** aHandler)=0;
  NS_IMETHOD SetPrototypeHandlers(nsIXBLPrototypeHandler* aHandler)=0;
  
  NS_IMETHOD GetPrototypeProperties(nsIXBLPrototypeProperty** aResult) = 0;
  NS_IMETHOD SetProtoTypeProperties(nsIXBLPrototypeProperty* aResult) = 0;
  NS_IMETHOD GetCompiledClassObject(const nsCString& aClassName, nsIScriptContext * aContext, void * aScriptObject, void ** aClassObject) = 0;
  
  NS_IMETHOD InitClass(const nsCString& aClassName, nsIScriptContext * aContext, void * aScriptObject, void ** aClassObject) = 0;
  
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag, 
                              nsIContent* aChangedElement, nsIContent* aAnonymousContent)=0;

  NS_IMETHOD SetBasePrototype(nsIXBLPrototypeBinding* aBinding)=0;
  NS_IMETHOD GetBasePrototype(nsIXBLPrototypeBinding** aResult)=0;

  NS_IMETHOD HasBasePrototype(PRBool* aResult)=0;
  NS_IMETHOD SetHasBasePrototype(PRBool aHasBase)=0;

  NS_IMETHOD GetXBLDocumentInfo(nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)=0;

  NS_IMETHOD SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)=0;

  NS_IMETHOD GetRuleProcessors(nsISupportsArray** aResult)=0;
  NS_IMETHOD GetStyleSheets(nsISupportsArray** aResult)=0;

  NS_IMETHOD HasInsertionPoints(PRBool* aResult)=0;
  NS_IMETHOD HasStyleSheets(PRBool* aResult)=0; 

  NS_IMETHOD InstantiateInsertionPoints(nsIXBLBinding* aBinding)=0;

  NS_IMETHOD GetInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                               nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex,
                               nsIContent** aDefaultContent)=0;

  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                                     nsIContent** aResult, PRUint32* aIndex, PRBool* aMultiple,
                                     nsIContent** aDefaultContent)=0;

  NS_IMETHOD GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aTag)=0;
  NS_IMETHOD SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag)=0;

  NS_IMETHOD ImplementsInterface(REFNSIID aIID, PRBool* aResult)=0;

  NS_IMETHOD ShouldBuildChildFrames(PRBool* aResult)=0;

  NS_IMETHOD AddResourceListener(nsIContent* aBoundElement)=0;

  NS_IMETHOD GetConstructor(nsIXBLPrototypeHandler** aResult)=0;

  NS_IMETHOD Initialize()=0;
};

extern nsresult
NS_NewXBLPrototypeBinding(const nsAReadableCString& aRef, 
                          nsIContent* aElement, nsIXBLDocumentInfo* aInfo, 
                          nsIXBLPrototypeBinding** aResult);

#endif // nsIXBLPrototypeBinding_h__
