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
#ifndef nsGenericXMLElement_h___
#define nsGenericXMLElement_h___

#include "nsGenericElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML
#include "nsINameSpace.h"

class nsIWebShell;

class nsGenericXMLElement : public nsGenericContainerElement {
public:
  nsGenericXMLElement();
  ~nsGenericXMLElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericXMLElement* aDest,
                       PRBool aDeep);

  nsresult GetNodeName(nsAWritableString& aNodeName);
  nsresult GetLocalName(nsAWritableString& aLocalName);

  // Implementation for nsIDOMElement
  nsresult    GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn) 
  {
    return nsGenericContainerElement::GetAttribute(aName, aReturn);
  }
  nsresult    SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
  {
    return nsGenericContainerElement::SetAttribute(aName, aValue);
  }

  // nsIContent
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        const nsAReadableString& aValue,
                        PRBool aNotify)
  {
    return nsGenericContainerElement::SetAttribute(aNameSpaceID, aName,
                                                   aValue, aNotify);
  }
  nsresult SetAttribute(nsINodeInfo *aNodeInfo,
                        const nsAReadableString& aValue,
                        PRBool aNotify)
  {
    return nsGenericContainerElement::SetAttribute(aNodeInfo, aValue, aNotify);
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        nsAWritableString& aResult) const
  {
    return nsGenericContainerElement::GetAttribute(aNameSpaceID, aName,
                                                   aResult);
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        nsIAtom*& aPrefix, nsAWritableString& aResult) const
  {
    return nsGenericContainerElement::GetAttribute(aNameSpaceID, aName,
                                                   aPrefix, aResult);
  }
  nsresult ParseAttributeString(const nsAReadableString& aStr, 
                                nsIAtom*& aName,
                                PRInt32& aNameSpaceID);
  nsresult GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                    nsIAtom*& aPrefix);


  // nsIXMLContent
  nsresult SetContainingNameSpace(nsINameSpace* aNameSpace);
  nsresult GetContainingNameSpace(nsINameSpace*& aNameSpace) const;

  nsresult SetNameSpacePrefix(nsIAtom* aNameSpace);
  nsresult GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
  nsresult GetNameSpaceID(PRInt32& aNameSpaceID) const;
  nsresult MaybeTriggerAutoLink(nsIWebShell *aShell);

  // nsIScriptObjectOwner
  nsresult GetScriptObject(nsIScriptContext* aContext, 
                           void** aScriptObject);

  nsINameSpace* mNameSpace;
};

#endif /* nsGenericXMLElement_h__ */
