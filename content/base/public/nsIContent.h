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
#ifndef nsIContent_h___
#define nsIContent_h___

#include <stdio.h>
#include "nslayout.h"
#include "nsISupports.h"
class nsIAtom;
class nsIContentDelegate;
class nsIDocument;
class nsIPresContext;
class nsISizeOfHandler;
class nsString;
class nsString;
class nsVoidArray;

// IID for the nsIContent interface
#define NS_ICONTENT_IID       \
{ 0x78030220, 0x9447, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * Content attribute states
 */
enum nsContentAttr {
  // Attribute does not exist on the piece of content
  eContentAttr_NotThere,

  // Attribute exists, but has no value, e.g. "BORDER" in <TABLE BORDER>
  eContentAttr_NoValue,

  // Attribute exists and has a value.  However, value may be the
  // empty string.  e.g. <TABLE BORDER="1"> or <TABLE BORDER="">
  eContentAttr_HasValue
};

// A node of content in a documents content model. This interface
// is supported by all content objects.
class nsIContent : public nsISupports
{
public:
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const = 0;
  virtual void SetDocument(nsIDocument* aDocument) = 0;

  virtual nsIContent* GetParent() const = 0;
  virtual void SetParent(nsIContent* aParent) = 0;

  virtual PRBool CanContainChildren() const = 0;
  virtual PRInt32 ChildCount() const = 0;
  virtual nsIContent* ChildAt(PRInt32 aIndex) const = 0;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const = 0;

  NS_IMETHOD InsertChildAt(nsIContent* aKid,
                           PRInt32 aIndex,
                           PRBool aNotify) = 0;

  NS_IMETHOD ReplaceChildAt(nsIContent* aKid,
                            PRInt32 aIndex,
                            PRBool aNotify) = 0;

  NS_IMETHOD AppendChild(nsIContent* aKid,
                         PRBool aNotify) = 0;

  NS_IMETHOD RemoveChildAt(PRInt32 aIndex,
                           PRBool aNotify) = 0;

  /**
   * Test and see if this piece of content is synthetic. Synthetic content
   * is content generated stylistically and should normally be ignored
   * during content saving.
   */
  NS_IMETHOD IsSynthetic(PRBool& aResult) = 0;

  virtual nsIAtom* GetTag() const = 0;

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical String representation that can be used for these
   * methods. The setAttribute method is assumed to perform a translation
   * of the canonical form into the underlying content specific
   * form.
   *
   * aValue may legitimately be the empty string.
   */
  virtual void SetAttribute(const nsString& aName, const nsString& aValue) = 0;

  /**
   * Get the current value of the attribute. This returns a form that is
   * suitable for passing back into setAttribute.
   *
   * <UL>
   *
   * <LI>If the attribute is not set and has no default value, return
   * eContentAttr_NotThere.
   *
   * <LI>If the attribute exists, but has no value, return
   * eContentAttr_NoValue.
   *
   * <LI>If the attribute has a value, empty or otherwise, set ret to
   * be the value, and return eContentAttr_HasValue.
   *
   * </UL> */
  virtual nsContentAttr GetAttribute(const nsString& aName,
                                     nsString& aResult) const = 0;

  virtual nsIContentDelegate* GetDelegate(nsIPresContext* aCX) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  /**
   * Add this object's size information to the sizeof handler and
   * any objects that it can reach.
   */
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const = 0;
};

#endif /* nsIContent_h___ */
