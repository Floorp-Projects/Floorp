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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIHTMLContent_h___
#define nsIHTMLContent_h___

#include "nsIStyledContent.h"
#include "nsHTMLValue.h"
class nsString;
class nsIFrame;
class nsIStyleRule;
class nsIMutableStyleContext;
class nsIPresContext;
class nsIXIFConverter;
class nsIHTMLMappedAttributes;
class nsIURI;

// IID for the nsIHTMLContent class
#define NS_IHTMLCONTENT_IID   \
{ 0xb9e110b0, 0x94d6, 0x11d1, \
  {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

typedef void (*nsMapAttributesFunc)(const nsIHTMLMappedAttributes* aAttributes, 
                                    nsIMutableStyleContext* aContext, 
                                    nsIPresContext* aPresContext);

// Abstract interface for all html content
class nsIHTMLContent : public nsIStyledContent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLCONTENT_IID; return iid; }

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
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                          nsMapAttributesFunc& aMapFunc) const = 0;

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

  /**
   * Translate this piece of content to html. Note that this only
   * translates this content object, not any children it might
   * have. The caller is responsible for recursing down the
   * hierarchy.
   */
  // XXX add in output character set information so that we know how
  // to encode non 7 bit characters
  NS_IMETHOD ToHTMLString(nsAWritableString& aResult) const = 0;

  NS_IMETHOD ToHTML(FILE* out) const = 0;
};

#endif /* nsIHTMLContent_h___ */
