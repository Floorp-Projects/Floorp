/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef __nsIEditProperty_h__
#define __nsIEditProperty_h__

#include "nsISupports.h"

class nsIAtom;
class nsString;

#define NS_IEDITPROPERTY_IID \
{/* 9875cd40-ca81-11d2-8f4d-006008159b0c*/ \
0x9875cd40, 0xca81, 0x11d2, \
{0x8f, 0x4d, 0x0, 0x60, 0x8, 0x15, 0x9b, 0x0c} }

/** simple interface for describing a single property as it relates to a range of content.
  *
  */

class nsIEditProperty : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITPROPERTY_IID; return iid; }

  NS_IMETHOD Init(nsIAtom *aPropName, nsIAtom *aValue, PRBool aAppliesToAll)=0;
  NS_IMETHOD GetProperty(nsIAtom **aProperty) const =0;  
  NS_IMETHOD GetValue(nsIAtom **aValue) const =0;  
  NS_IMETHOD GetAppliesToAll(PRBool *aAppliesToAll) const =0;  

/* we're still trying to decide how edit atoms will work.  Until then, use these */
// XXX: fix ASAP!
  // inline tags
  static nsIAtom *a;
  static nsIAtom *b;
  static nsIAtom *big;
  static nsIAtom *font;
  static nsIAtom *i;
  static nsIAtom *span;
  static nsIAtom *small;
  static nsIAtom *strike;
  static nsIAtom *sub;
  static nsIAtom *sup;
  static nsIAtom *tt;
  static nsIAtom *u;

/* from HTML 3.2 spec
TT teletype or monospaced text 
I italic text style 
B bold text style 
U underlined text style 
STRIKE strike-through text style 
BIG places text in a large font 
SMALL places text in a small font 
SUB places text in subscript style 
SUP places text in superscript style 
*/

  // block tags 
  static nsIAtom *blockquote;
  static nsIAtom *h1;
  static nsIAtom *h2;


  /** properties */
  static nsIAtom *color;
  static nsIAtom *face;
  static nsIAtom *size;

  /** special strings */
  static nsString *allProperties;   // this magic string represents the union of all inline style tags

  // XXX: end temp code
};

extern nsresult NS_NewEditProperty(nsIEditProperty **aResult);

#endif
