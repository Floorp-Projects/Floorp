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
#ifndef nsGenericXMLElement_h___
#define nsGenericXMLElement_h___

#include "nsGenericElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML
#include "nsINameSpace.h"

class nsGenericXMLElement : public nsGenericContainerElement {
public:
  nsGenericXMLElement();
  ~nsGenericXMLElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericXMLElement* aDest,
                       PRBool aDeep);

  // Implementation for nsIDOMElement
  nsresult    GetAttribute(const nsString& aName, nsString& aReturn) 
  {
    return nsGenericContainerElement::GetAttribute(aName, aReturn);
  }
  nsresult    SetAttribute(const nsString& aName, const nsString& aValue)
  {
    return nsGenericContainerElement::SetAttribute(aName, aValue);
  }

  // nsIContent
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        const nsString& aValue,
                        PRBool aNotify)
  {
    return nsGenericContainerElement::SetAttribute(aNameSpaceID, aName,
                                                   aValue, aNotify);
  }
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        nsString& aResult) const
  {
    return nsGenericContainerElement::GetAttribute(aNameSpaceID, aName,
                                                   aResult);
  }
  nsresult ParseAttributeString(const nsString& aStr, 
                                nsIAtom*& aName,
                                PRInt32& aNameSpaceID);
  nsresult GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                    nsIAtom*& aPrefix);


  // nsIXMLContent
  nsresult SetContainingNameSpace(nsINameSpace* aNameSpace);
  nsresult GetContainingNameSpace(nsINameSpace*& aNameSpace) const;

  nsresult SetNameSpacePrefix(nsIAtom* aNameSpace);
  nsresult GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
  nsresult SetNameSpaceID(PRInt32 aNameSpaceId);
  nsresult GetNameSpaceID(PRInt32& aNameSpaceID) const;

  // nsIScriptObjectOwner
  nsresult GetScriptObject(nsIScriptContext* aContext, 
                           void** aScriptObject);



  nsINameSpace* mNameSpace;
  nsIAtom* mNameSpacePrefix;
  PRInt32 mNameSpaceID;
};

#endif /* nsGenericXMLElement_h__ */
