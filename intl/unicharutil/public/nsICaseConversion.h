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
#ifndef nsICaseConversion_h__
#define nsICaseConversion_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"

// {07D3D8E0-9614-11d2-B3AD-00805F8A6670}
#define NS_ICASECONVERSION_IID \
{ 0x7d3d8e0, 0x9614, 0x11d2, \
    { 0xb3, 0xad, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

class nsICaseConversion : public nsISupports {

public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICASECONVERSION_IID)

  // Convert one Unicode character into upper case
  NS_IMETHOD ToUpper( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert one Unicode character into lower case
  NS_IMETHOD ToLower( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert one Unicode character into title case
  NS_IMETHOD ToTitle( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert an array of Unicode characters into upper case
  NS_IMETHOD ToUpper( const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen) = 0;

  // Convert an array of Unicode characters into lower case
  NS_IMETHOD ToLower( const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen) = 0;

  // Convert an array of Unicode characters into title case
  NS_IMETHOD ToTitle( const PRUnichar* anArray, PRUnichar* aReturn, 
                      PRUint32 aLen, PRBool aStartInWordBundary=PR_TRUE) = 0;

  // The following nsString flavor one know how to handle special casing
  NS_IMETHOD ToUpper(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull) = 0;
  NS_IMETHOD ToLower(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull ) = 0;
  NS_IMETHOD ToTitle(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull, PRBool aStartInWordBoundary=PR_TRUE) = 0;
   
};

#endif  /* nsICaseConversion_h__ */
