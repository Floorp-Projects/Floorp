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

#ifndef nsIXMLContent_h___
#define nsIXMLContent_h___

#include "nsISupports.h"
#include "nsIStyledContent.h"

class nsINameSpace;
class nsINodeInfo;
class nsIWebShell;

#define NS_IXMLCONTENT_IID \
 { 0xa6cf90cb, 0x15b3, 0x11d2, \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

/**
 * XML content extensions to nsIContent
 */
class nsIXMLContent : public nsIStyledContent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXMLCONTENT_IID; return iid; }
  
  NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace) = 0;
  NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const = 0;
  
  NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace) = 0;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const = 0;

  /**
   * Give this element a change to fire its links that should be fired
   * automatically when loaded. If the element was an autoloading link
   * and it was succesfully handled, we will return informal return value.
   * If the return value is NS_XML_AUTOLINK_REPLACE, the caller should
   * stop processing the current document because it will be replaced.
   * We normally treat NS_XML_AUTOLINK_UNDEFINED the same way as replace
   * links, so processing should usually stop after that as well.
   */
  NS_IMETHOD MaybeTriggerAutoLink(nsIWebShell *aShell) = 0;
};

// Some return values for MaybeTriggerAutoLink
#define NS_XML_AUTOLINK_EMBED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 4)
#define NS_XML_AUTOLINK_NEW \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 5)
#define NS_XML_AUTOLINK_REPLACE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 6)
#define NS_XML_AUTOLINK_UNDEFINED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 7)

extern nsresult
NS_NewXMLElement(nsIXMLContent** aResult, nsINodeInfo* aNodeInfo);

// XXX These belongs elsewhere
extern nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsAReadableString& aTarget,
                               const nsAReadableString& aData);

extern nsresult
NS_NewXMLEntity(nsIContent** aInstancePtrResult,
                const nsAReadableString& aName,
                const nsAReadableString& aPublicId,
                const nsAReadableString& aSystemId,
                const nsAReadableString& aNotationName);

extern nsresult
NS_NewXMLNotation(nsIContent** aInstancePtrResult,
                  const nsAReadableString& aName,
                  const nsAReadableString& aPublicId,
                  const nsAReadableString& aSystemId);

class nsIDOMNamedNodeMap;

extern nsresult
NS_NewXMLDocumentType(nsIContent** aInstancePtrResult,
                      const nsAReadableString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations,
                      const nsAReadableString& aPublicId,
                      const nsAReadableString& aSystemId,
                      const nsAReadableString& aInternalSubset);

extern nsresult
NS_NewXMLNamedNodeMap(nsIDOMNamedNodeMap** aInstancePtrResult,
                      nsISupportsArray *aArray);

extern nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult);

#endif // nsIXMLContent_h___
