/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef nsUnicharUtils_h__
#define nsUnicharUtils_h__
#ifndef nsAString_h___
#include "nsAString.h"
#endif

class nsASingleFragmentString;
class nsString;

void ToLowerCase( nsAString& );
void ToUpperCase( nsAString& );

void ToLowerCase( nsASingleFragmentString& );
void ToUpperCase( nsASingleFragmentString& );

void ToLowerCase( nsString& );
void ToUpperCase( nsString& );

void ToLowerCase( const nsAString& aSource, nsAString& aDest );
void ToUpperCase( const nsAString& aSource, nsAString& aDest );

PRBool CaseInsensitiveFindInReadable( const nsAString& aPattern, nsAString::const_iterator&, nsAString::const_iterator& );

class nsCaseInsensitiveStringComparator
    : public nsStringComparator
  {
    public:
      virtual int operator()( const PRUnichar*, const PRUnichar*, PRUint32 aLength ) const;
      virtual int operator()( PRUnichar, PRUnichar ) const;
  };


PRUnichar ToUpperCase(PRUnichar);
PRUnichar ToLowerCase(PRUnichar);

inline PRBool IsUpperCase(PRUnichar c) {
    return ToUpperCase(c) == c;
}

inline PRBool IsLowerCase(PRUnichar c) {
    return ToLowerCase(c) == c;
}

#define IS_HIGH_SURROGATE(u)  ((PRUnichar)(u) >= (PRUnichar)0xd800 && (PRUnichar)(u) <= (PRUnichar)0xdbff)
#define IS_LOW_SURROGATE(u)  ((PRUnichar)(u) >= (PRUnichar)0xdc00 && (PRUnichar)(u) <= (PRUnichar)0xdfff)

#define SURROGATE_TO_UCS4(h, l)  ((((PRUint32)(h)-(PRUint32)0xd800) << 10) +  \
                                    (PRUint32)(l) - (PRUint32)(0xdc00) + 0x10000)


#endif  /* nsUnicharUtils_h__ */
