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

#ifndef nsIXMLContent_h___
#define nsIXMLContent_h___

#include "nsISupports.h"
#include "nsIContent.h"

class nsINameSpace;

#define NS_IXMLCONTENT_IID \
 { 0xa6cf90cb, 0x15b3, 0x11d2, \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

/**
 * XML content extensions to nsIContent
 */
class nsIXMLContent : public nsIContent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXMLCONTENT_IID; return iid; }
  
  NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace) = 0;
  NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const = 0;
  
  NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace) = 0;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const = 0;

  NS_IMETHOD SetNameSpaceID(PRInt32 aNSIdentifier) = 0;
};

extern nsresult
NS_NewXMLElement(nsIXMLContent** aResult, nsIAtom* aTag);

// XXX These belongs elsewhere
extern nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsString& aTarget,
                               const nsString& aData);

extern nsresult
NS_NewXMLEntity(nsIContent** aInstancePtrResult,
                const nsString& aName,
                const nsString& aPublicId,
                const nsString& aSystemId,
                const nsString aNotationName);

extern nsresult
NS_NewXMLNotation(nsIContent** aInstancePtrResult,
                  const nsString& aName,
                  const nsString& aPublicId,
                  const nsString& aSystemId);

class nsIDOMNamedNodeMap;

extern nsresult
NS_NewXMLDocumentType(nsIContent** aInstancePtrResult,
                      const nsString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations);

extern nsresult
NS_NewXMLNamedNodeMap(nsIDOMNamedNodeMap** aInstancePtrResult,
                      nsISupportsArray *aArray);

extern nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult);

#endif // nsIXMLContent_h___
