/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsStringGlue.h"

/* (0x3131u <= (u) && (u) <= 0x318eu) => Hangul Compatibility Jamo */
/* (0xac00u <= (u) && (u) <= 0xd7a3u) => Hangul Syllables          */
#define IS_CJ_CHAR(u) \
  ((0x2e80u <= (u) && (u) <= 0x312fu) || \
   (0x3190u <= (u) && (u) <= 0xabffu) || \
   (0xf900u <= (u) && (u) <= 0xfaffu) || \
   (0xff00u <= (u) && (u) <= 0xffefu) )

void ToLowerCase(nsAString&);
void ToUpperCase(nsAString&);

void ToLowerCase(const nsAString& aSource, nsAString& aDest);
void ToUpperCase(const nsAString& aSource, nsAString& aDest);

PRUnichar ToLowerCase(PRUnichar);
PRUnichar ToUpperCase(PRUnichar);
PRUnichar ToTitleCase(PRUnichar);

void ToLowerCase(const PRUnichar*, PRUnichar*, PRUint32);
void ToUpperCase(const PRUnichar*, PRUnichar*, PRUint32);

inline bool IsUpperCase(PRUnichar c) {
  return ToLowerCase(c) != c;
}

inline bool IsLowerCase(PRUnichar c) {
  return ToUpperCase(c) != c;
}

#ifdef MOZILLA_INTERNAL_API

class nsCaseInsensitiveStringComparator : public nsStringComparator
{
public:
  virtual PRInt32 operator() (const PRUnichar*,
                              const PRUnichar*,
                              PRUint32,
                              PRUint32) const;
};

class nsCaseInsensitiveUTF8StringComparator : public nsCStringComparator
{
public:
  virtual PRInt32 operator() (const char*,
                              const char*,
                              PRUint32,
                              PRUint32) const;
};

class nsCaseInsensitiveStringArrayComparator
{
public:
  template<class A, class B>
  bool Equals(const A& a, const B& b) const {
    return a.Equals(b, nsCaseInsensitiveStringComparator());
  }
};

class nsASCIICaseInsensitiveStringComparator : public nsStringComparator
{
public:
  nsASCIICaseInsensitiveStringComparator() {}
  virtual int operator() (const PRUnichar*,
                          const PRUnichar*,
                          PRUint32,
                          PRUint32) const;
};

inline bool
CaseInsensitiveFindInReadable(const nsAString& aPattern,
                              nsAString::const_iterator& aSearchStart,
                              nsAString::const_iterator& aSearchEnd)
{
  return FindInReadable(aPattern, aSearchStart, aSearchEnd,
                        nsCaseInsensitiveStringComparator());
}

inline bool
CaseInsensitiveFindInReadable(const nsAString& aPattern,
                              const nsAString& aHay)
{
  nsAString::const_iterator searchBegin, searchEnd;
  return FindInReadable(aPattern, aHay.BeginReading(searchBegin),
                        aHay.EndReading(searchEnd),
                        nsCaseInsensitiveStringComparator());
}

#endif // MOZILLA_INTERNAL_API

PRInt32
CaseInsensitiveCompare(const PRUnichar *a, const PRUnichar *b, PRUint32 len);

PRInt32
CaseInsensitiveCompare(const char* aLeft, const char* aRight,
                       PRUint32 aLeftBytes, PRUint32 aRightBytes);

/**
 * This function determines whether the UTF-8 sequence pointed to by aLeft is
 * case-insensitively-equal to the UTF-8 sequence pointed to by aRight.
 *
 * aLeftEnd marks the first memory location past aLeft that is not part of
 * aLeft; aRightEnd similarly marks the end of aRight.
 *
 * The function assumes that aLeft < aLeftEnd and aRight < aRightEnd.
 *
 * The function stores the addresses of the next characters in the sequence
 * into aLeftNext and aRightNext.  It's up to the caller to make sure that the
 * returned pointers are valid -- i.e. the function may return aLeftNext >=
 * aLeftEnd or aRightNext >= aRightEnd.
 *
 * If the function encounters invalid text, it sets aErr to true and returns
 * false, possibly leaving aLeftNext and aRightNext uninitialized.  If the
 * function returns true, aErr is guaranteed to be false and both aLeftNext and
 * aRightNext are guaranteed to be initialized.
 */
bool
CaseInsensitiveUTF8CharsEqual(const char* aLeft, const char* aRight,
                              const char* aLeftEnd, const char* aRightEnd,
                              const char** aLeftNext, const char** aRightNext,
                              bool* aErr);

namespace mozilla {

/**
 * Hash a UTF8 string as though it were a UTF16 string.
 *
 * The value returned is the same as if we converted the string to UTF16 and
 * then ran HashString() on the result.
 *
 * The given |length| is in bytes.
 */
PRUint32
HashUTF8AsUTF16(const char* aUTF8, PRUint32 aLength, bool* aErr);

} // namespace mozilla

#endif  /* nsUnicharUtils_h__ */
