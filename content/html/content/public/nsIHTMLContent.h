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
class nsISupportsArray;
class nsIStyleRule;
class nsIStyleContext;
class nsIPresContext;

// IID for the nsIHTMLContent class
#define NS_IHTMLCONTENT_IID   \
{ 0xb9e110b0, 0x94d6, 0x11d1, \
  {0x89, 0x5c, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

// Abstract interface for all html content
class nsIHTMLContent : public nsIContent {
public:
  /**
   * If this html content is a container, then compact asks it to minimize
   * it's storage usage.
   */
  virtual void Compact() = 0;

  virtual void SetAttribute(const nsString& aName, const nsString& aValue) = 0;
  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue) = 0;
  virtual void SetAttribute(nsIAtom* aAttribute,
                            const nsHTMLValue& aValue=nsHTMLValue::kNull) = 0;

  virtual void UnsetAttribute(nsIAtom* aAttribute) = 0;

  virtual nsContentAttr GetAttribute(const nsString& aName,
                                     nsString& aResult) const = 0;
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const = 0;
  virtual PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const = 0;
  virtual PRInt32 GetAttributeCount(void) const = 0;

  virtual void      SetID(nsIAtom* aID) = 0;
  virtual nsIAtom*  GetID(void) const = 0;
  virtual void      SetClass(nsIAtom* aClass) = 0;  // XXX this will have to change for CSS2
  virtual nsIAtom*  GetClass(void) const = 0;  // XXX this will have to change for CSS2

  virtual nsIStyleRule* GetStyleRule(void) = 0;

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext) = 0;

  /**
   * Translate this piece of content to html. Note that this only
   * translates this content object, not any children it might
   * have. The caller is responsible for recursing down the
   * hierarchy.
   */
  // XXX add in output character set information so that we know how
  // to encode non 7 bit characters
  virtual void ToHTMLString(nsString& aResult) const = 0;

  virtual void ToHTML(FILE* out) const = 0;

  /**
   * Used by the html content's delegate to create a frame
   * for the content.
   */
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult) = 0;
};

#endif /* nsIHTMLContent_h___ */
