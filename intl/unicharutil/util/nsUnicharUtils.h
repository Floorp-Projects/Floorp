/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Unicode case conversion helpers.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUnicharUtils_h__
#define nsUnicharUtils_h__
#ifndef nsAString_h___
#include "nsAString.h"
#endif

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
    return ToLowerCase(c) != c;
}

inline PRBool IsLowerCase(PRUnichar c) {
    return ToUpperCase(c) != c;
}

#define IS_HIGH_SURROGATE(u)  ((PRUnichar)(u) >= (PRUnichar)0xd800 && (PRUnichar)(u) <= (PRUnichar)0xdbff)
#define IS_LOW_SURROGATE(u)  ((PRUnichar)(u) >= (PRUnichar)0xdc00 && (PRUnichar)(u) <= (PRUnichar)0xdfff)

#define SURROGATE_TO_UCS4(h, l)  ((((PRUint32)(h)-(PRUint32)0xd800) << 10) +  \
                                    (PRUint32)(l) - (PRUint32)(0xdc00) + 0x10000)

#define H_SURROGATE(s) ((PRUnichar)(((PRUint32)s - (PRUint32)0x10000) >> 10) + (PRUnichar)0xd800)
#define L_SURROGATE(s) ((PRUnichar)(((PRUint32)s - (PRUint32)0x10000) & 0x3ff) + (PRUnichar)0xdc00)
#define IS_IN_BMP(ucs) ((PRUint32)ucs < 0x10000)

/* (0x3131u <= (u) && (u) <= 0x318eu) => Hangul Compatibility Jamo */
/* (0xac00u <= (u) && (u) <= 0xd7a3u) => Hangul Syllables          */
#define IS_CJ_CHAR(u) \
         ((0x2e80u <= (u) && (u) <= 0x312fu) || \
          (0x3190u <= (u) && (u) <= 0xabffu) || \
          (0xf900u <= (u) && (u) <= 0xfaffu) || \
          (0xff00u <= (u) && (u) <= 0xffefu) )

#endif  /* nsUnicharUtils_h__ */
