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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIHTMLStyleSheet_h___
#define nsIHTMLStyleSheet_h___

#include "nslayout.h"
#include "nsIStyleSheet.h"
#include "nsColor.h"

class nsIAtom;
class nsString;
class nsHTMLValue;
class nsIHTMLAttributes;
class nsIHTMLMappedAttributes;
class nsIHTMLContent;
class nsIDocument;

// IID for the nsIHTMLStyleSheet interface {bddbd1b0-c5cc-11d1-8031-006008159b5a}
#define NS_IHTML_STYLE_SHEET_IID     \
{0xbddbd1b0, 0xc5cc, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIHTMLStyleSheet : public nsIStyleSheet {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTML_STYLE_SHEET_IID)

  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument) = 0;
  NS_IMETHOD Reset(nsIURI* aURL) = 0;
  NS_IMETHOD GetLinkColor(nscolor& aColor) = 0;
  NS_IMETHOD SetLinkColor(nscolor aColor) = 0;
  NS_IMETHOD GetActiveLinkColor(nscolor& aColor) = 0;
  NS_IMETHOD SetActiveLinkColor(nscolor aColor) = 0;
  NS_IMETHOD GetVisitedLinkColor(nscolor& aColor) = 0;
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor) = 0;
  NS_IMETHOD GetDocumentForegroundColor(nscolor& aColor) = 0;
  NS_IMETHOD SetDocumentForegroundColor(nscolor aColor) = 0;
  NS_IMETHOD GetDocumentBackgroundColor(nscolor& aColor) = 0;
  NS_IMETHOD SetDocumentBackgroundColor(nscolor aColor) = 0;

  // Attribute management methods
  NS_IMETHOD SetAttributesFor(nsIHTMLContent* aContent, 
                              nsIHTMLAttributes*& aAttributes) = 0;
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, 
                             const nsAReadableString& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes) = 0;
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes) = 0;
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, nsIHTMLContent* aContent, 
                               nsIHTMLAttributes*& aAttributes) = 0;

  // Mapped Attribute management methods
  NS_IMETHOD UniqueMappedAttributes(nsIHTMLMappedAttributes* aMapped,
                                    nsIHTMLMappedAttributes*& aUniqueMapped) = 0;
  NS_IMETHOD DropMappedAttributes(nsIHTMLMappedAttributes* aMapped) = 0;
};


// XXX convenience method. Calls Initialize() automatically.
extern NS_HTML nsresult
  NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                       nsIDocument* aDocument);

extern NS_HTML nsresult
  NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult);

#define NS_HTML_STYLE_PROPERTY_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT,2)

#endif /* nsIHTMLStyleSheet_h___ */
