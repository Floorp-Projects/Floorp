/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Peter Annema <disttsc@bart.nl>
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

#ifndef nsIXMLContent_h___
#define nsIXMLContent_h___

#include "nsISupports.h"
#include "nsIStyledContent.h"

class nsINameSpace;
class nsINodeInfo;
class nsIWebShell;
class nsIURI;

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

  NS_IMETHOD GetXMLBaseURI(nsIURI **aURI) = 0;
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
NS_NewXMLNamedNodeMap(nsIDOMNamedNodeMap** aInstancePtrResult,
                      nsISupportsArray *aArray);

extern nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult);

#endif // nsIXMLContent_h___
