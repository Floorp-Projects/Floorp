/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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


/*

  A reference-counted XPCOM wrapper for the nsStr structure. Allows for
  efficient passing of string objects through multiple layers.

 */

#ifndef nsIString_h__
#define nsIString_h__

#include "nsISupports.h"
struct nsStr;

// {4C541410-E0B9-11d2-BDB3-000064657374}
#define NS_ISTRING_IID \
{ 0x4c541410, 0xe0b9, 0x11d2, { 0xbd, 0xb3, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsIString : public nsISupports
{
public:
  static const nsID& GetIID() { static nsIID iid = NS_ISTRING_IID; return iid; }

  /** 
   * Copy the caller's nsStr into the object's internal nsStr. The copy is "deep"; 
   * that is, it duplicates the contents of "aStr.mStr" as well. 
   */ 
  NS_IMETHOD SetStr(nsStr* aStr) = 0;

  /** 
   * Copy the object's internal nsStr into the caller's nsStr struct. The copy is 
   * "deep"; that is, it duplicates the nsStr::mStr member, as well. 
   */ 
  NS_IMETHOD GetStr(nsStr* aStr) = 0 

  /** 
   * Return a pointer to the nsIString object's _actual_ string buffer. The caller 
   * must treat this struct as an immutable object (hence the "const" qualifier)
   * The pointer will become invalid as soon as the nsIString interface is released.
   */
  NS_IMETHOD GetImmutableStr(const nsStr** aStr) = 0;
};


PR_EXTERN(nsresult)
NS_NewString(nsIString** aString);

#endif // nsIString_h__

