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
#ifndef nsIHTMLAttributes_h___
#define nsIHTMLAttributes_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsHTMLValue.h"
#include "nsIContent.h"
class nsIAtom;
class nsISupportsArray;
class nsIHTMLContent;


// IID for the nsIHTMLAttributes interface {a18f85f0-c058-11d1-8031-006008159b5a}
#define NS_IHTML_ATTRIBUTES_IID     \
{0xa18f85f0, 0xc058, 0x11d1,        \
  {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIHTMLAttributes : public nsISupports {
public:
  virtual PRInt32 SetAttribute(nsIAtom* aAttribute, const nsString& aValue) = 0;
  virtual PRInt32 SetAttribute(nsIAtom* aAttribute, 
                               const nsHTMLValue& aValue = nsHTMLValue::kNull) = 0;
  virtual PRInt32 UnsetAttribute(nsIAtom* aAttribute) = 0;
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const = 0;
  virtual PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const = 0;

  virtual PRInt32 Count(void) const = 0;

  virtual PRInt32   SetID(nsIAtom* aID) = 0;
  virtual nsIAtom*  GetID(void) const = 0;
  virtual PRInt32   SetClass(nsIAtom* aClass) = 0;  // XXX this will have to change for CSS2
  virtual nsIAtom*  GetClass(void) const = 0;  // XXX this will have to change for CSS2

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

extern NS_HTML nsresult
  NS_NewHTMLAttributes(nsIHTMLAttributes** aInstancePtrResult, nsIHTMLContent* aContent);

#endif /* nsIHTMLAttributes_h___ */

