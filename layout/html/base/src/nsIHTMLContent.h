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
#ifndef nsIHTMLContent_h___
#define nsIHTMLContent_h___

#include "nsIXMLContent.h"
#include "nsHTMLValue.h"
class nsString;
class nsIFrame;
class nsIStyleRule;
class nsIPresContext;
class nsIHTMLMappedAttributes;
class nsIURI;
struct nsRuleData;

// IID for the nsIHTMLContent class
#define NS_IHTMLCONTENT_IID   \
{ 0xb9e110b0, 0x94d6, 0x11d1, \
  {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

typedef void (*nsMapRuleToAttributesFunc)(const nsIHTMLMappedAttributes* aAttributes, 
                                          nsRuleData* aData);

// Abstract interface for all html content
class nsIHTMLContent : public nsIXMLContent
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLCONTENT_IID)

  /**
   * If this html content is a container, then compact asks it to minimize
   * it's storage usage.
   */
  NS_IMETHOD Compact() = 0;

  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              PRBool aNotify) = 0;

  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute,
                              nsHTMLValue& aValue) const = 0;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const = 0;

  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const = 0;

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult) = 0;

  /**
   * Get the base URL for any relative URLs within this piece
   * of content. Generally, this is the document's base URL,
   * but certain content carries a local base for backward
   * compatibility.
   */
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const = 0;

  /**
   * Get the base target for any links within this piece
   * of content. Generally, this is the document's base target,
   * but certain content carries a local base for backward
   * compatibility.
   */
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const = 0;
};

#endif /* nsIHTMLContent_h___ */
