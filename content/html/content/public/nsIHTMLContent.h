/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIHTMLContent_h___
#define nsIHTMLContent_h___

#include "nsIContent.h"
#include "nsHTMLValue.h"
class nsString;
class nsIFrame;
class nsIStyleRule;
class nsIStyleContext;
class nsIPresContext;
class nsXIFConverter;
class nsIHTMLAttributes;

// IID for the nsIHTMLContent class
#define NS_IHTMLCONTENT_IID   \
{ 0xb9e110b0, 0x94d6, 0x11d1, \
  {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

typedef void (*nsMapAttributesFunc)(nsIHTMLAttributes* aAttributes, 
                                    nsIStyleContext* aContext, 
                                    nsIPresContext* aPresContext);

// Abstract interface for all html content
class nsIHTMLContent : public nsIContent {
public:
  static const nsIID& IID() { static nsIID iid = NS_IHTMLCONTENT_IID; return iid; }

  /**
   * If this html content is a container, then compact asks it to minimize
   * it's storage usage.
   */
  NS_IMETHOD Compact() = 0;

  NS_IMETHOD SetAttribute(const nsString& aName, const nsString& aValue,
                          PRBool aNotify) = 0;
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify) = 0;
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute,
                          const nsHTMLValue& aValue,
                          PRBool aNotify) = 0;

  NS_IMETHOD GetAttribute(const nsString& aName, 
                          nsString& aResult) const = 0;
  NS_IMETHOD GetAttribute(nsIAtom *aAttribute,
                          nsString &aResult) const = 0;
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aValue) const = 0;
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const = 0;

  NS_IMETHOD SetID(nsIAtom* aID) = 0;
  NS_IMETHOD GetID(nsIAtom*& aResult) const = 0;
  // XXX this will have to change for CSS2
  NS_IMETHOD SetClass(nsIAtom* aClass) = 0;
  // XXX this will have to change for CSS2
  NS_IMETHOD GetClass(nsIAtom*& aResult) const = 0;

  NS_IMETHOD GetContentStyleRule(nsIStyleRule*& aResult) = 0;
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule*& aResult) = 0;

  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const = 0;

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult) = 0;

  /**
   * Translate this piece of content to html. Note that this only
   * translates this content object, not any children it might
   * have. The caller is responsible for recursing down the
   * hierarchy.
   */
  // XXX add in output character set information so that we know how
  // to encode non 7 bit characters
  NS_IMETHOD ToHTMLString(nsString& aResult) const = 0;

  NS_IMETHOD ToHTML(FILE* out) const = 0;

  /** NRA ***
   * Get a hint that tells the style system what to do when 
   * an attribute on this node changes.
   */
  NS_IMETHOD GetStyleHintForAttributeChange(
    const nsIContent * aNode,
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const = 0;

};

#endif /* nsIHTMLContent_h___ */
