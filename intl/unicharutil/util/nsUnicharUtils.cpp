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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *   Ben Turner <mozilla@songbirdnest.com>
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

#include "nsUnicharUtils.h"
#include "nsUnicharUtilCIID.h"

#include "nsCRT.h"
#include "nsICaseConversion.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMStrings.h"
#include "casetable.h"
#include "nsUTF8Utils.h"
#include "nsHashKeys.h"

#include <ctype.h>

// For gUpperToTitle
enum {
  kUpperIdx =0,
  kTitleIdx
};

// For gUpperToTitle
enum {
  kLowIdx =0,
  kSizeEveryIdx,
  kDiffIdx
};

#define IS_ASCII(u)       ((u) < 0x80)
#define IS_ASCII_UPPER(u) (('A' <= (u)) && ( (u) <= 'Z' ))
#define IS_ASCII_LOWER(u) (('a' <= (u)) && ( (u) <= 'z'))
#define IS_ASCII_ALPHA(u) (IS_ASCII_UPPER(u) || IS_ASCII_LOWER(u))
#define IS_ASCII_SPACE(u) ( ' ' == (u) )

#define IS_NOCASE_CHAR(u)  (0==(1&(gCaseBlocks[(u)>>13]>>(0x001F&((u)>>8)))))

// Size of Tables

// Changing these numbers may break UTF-8 caching.  Be careful!
#define CASE_MAP_CACHE_SIZE 0x100
#define CASE_MAP_CACHE_MASK 0xFF

struct nsCompressedMap {
  const PRUnichar *mTable;
  PRUint32 mSize;
  PRUint32 mCache[CASE_MAP_CACHE_SIZE];
  PRUint32 mLastBase;

  PRUnichar Map(PRUnichar aChar)
  {
    // We don't need explicit locking here since the cached values are int32s,
    // which are read and written atomically.  The following code is threadsafe
    // because we never access bits from mCache directly -- we always first
    // read the entire entry into a local variable and then mask off the bits
    // we're interested in.

    // Check the 256-byte cache first and bail with our answer if we can.
    PRUint32 cachedData = mCache[aChar & CASE_MAP_CACHE_MASK];
    if (aChar == ((cachedData >> 16) & 0x0000FFFF))
      return cachedData & 0x0000FFFF;

    // Now try the last index we looked up, storing it into a local variable
    // for thread-safety.
    PRUint32 base = mLastBase;
    PRUnichar res = 0;

    // Does this character fit in the slot?
    if ((aChar <= ((mTable[base+kSizeEveryIdx] >> 8) +
                   mTable[base+kLowIdx])) &&
        (mTable[base+kLowIdx] <= aChar)) {

      // This character uses the same base as our last lookup, so the
      // conversion is easy.
      if (((mTable[base+kSizeEveryIdx] & 0x00FF) > 0) &&
          (0 != ((aChar - mTable[base+kLowIdx]) % 
                 (mTable[base+kSizeEveryIdx] & 0x00FF))))
      {
        res = aChar;
      } else {
        res = aChar + mTable[base+kDiffIdx];
      }

    } else {
      // Do the full lookup.
      res = this->Lookup(0, mSize/2, mSize-1, aChar);
    }

    // Cache the result and return.
    mCache[aChar & CASE_MAP_CACHE_MASK] =
        ((aChar << 16) & 0xFFFF0000) | (0x0000FFFF & res);
    return res;
  }

  // Takes as arguments the left bound, middle, right bound, and character to
  // search for.  Executes a binary search.
  PRUnichar Lookup(PRUint32 l,
                   PRUint32 m,
                   PRUint32 r,
                   PRUnichar aChar)
  {
    PRUint32 base = m*3; // Every line in the table is 3 units wide.

    // Is aChar past the top of the current table entry?  (The upper byte of
    // the 'every' entry contains the offset to the end of this entry.)
    if (aChar > ((mTable[base+kSizeEveryIdx] >> 8) + 
                  mTable[base+kLowIdx])) 
    {
      if (l > m || l == r)
        return aChar;
      // Advance one round.
      PRUint32 newm = (m+r+1)/2;
      if (newm == m)
        newm++;
      return this->Lookup(m+1, newm, r, aChar);

    // Is aChar below the bottom of the current table entry?
    } else if (mTable[base+kLowIdx] > aChar) {
      if (r < m || l == r)
        return aChar;
      // Advance one round
      PRUint32 newm = (l+m-1)/2;
      if(newm == m)
        newm++;
      return this->Lookup(l, newm, m-1, aChar);

    // We've found the entry aChar should live in.
    } else {
      // Determine if aChar falls in a gap.  (The lower byte of the 'every'
      // entry contains n for which every nth character from the base is a
      // character of interest.)
      if (((mTable[base+kSizeEveryIdx] & 0x00FF) > 0) && 
          (0 != ((aChar - mTable[base+kLowIdx]) % 
                 (mTable[base+kSizeEveryIdx] & 0x00FF))))
      {
        return aChar;
      }
      // If aChar doesn't fall in the gap, cache and convert.
      mLastBase = base;
      return aChar + mTable[base+kDiffIdx];
    }
  }
};

static nsCompressedMap gUpperMap = {
  reinterpret_cast<const PRUnichar*>(&gToUpper[0]),
  gToUpperItems
};

static nsCompressedMap gLowerMap = {
  reinterpret_cast<const PRUnichar*>(&gToLower[0]),
  gToLowerItems
};

// We want ToLowerCase(PRUnichar) and ToLowerCaseASCII(PRUnichar) to be fast
// when they're called from within the case-insensitive comparators, so we
// define inlined versions.
static NS_ALWAYS_INLINE PRUnichar
ToLowerCase_inline(PRUnichar aChar)
{
  if (IS_ASCII(aChar)) {
    return gASCIIToLower[aChar];
  } else if (IS_NOCASE_CHAR(aChar)) {
     return aChar;
  }

  return gLowerMap.Map(aChar);
}

static NS_ALWAYS_INLINE PRUnichar
ToLowerCaseASCII_inline(const PRUnichar aChar)
{
  if (IS_ASCII(aChar))
    return gASCIIToLower[aChar];
  return aChar;
}

void
ToLowerCase(nsAString& aString)
{
  PRUnichar *buf = aString.BeginWriting();
  ToLowerCase(buf, buf, aString.Length());
}

void
ToLowerCase(const nsAString& aSource,
            nsAString& aDest)
{
  const PRUnichar *in;
  PRUnichar *out;
  PRUint32 len = NS_StringGetData(aSource, &in);
  NS_StringGetMutableData(aDest, len, &out);
  NS_ASSERTION(out, "Uh...");
  ToLowerCase(in, out, len);
}

PRUnichar
ToLowerCaseASCII(const PRUnichar aChar)
{
  return ToLowerCaseASCII_inline(aChar);
}

void
ToUpperCase(nsAString& aString)
{
  PRUnichar *buf = aString.BeginWriting();
  ToUpperCase(buf, buf, aString.Length());
}

void
ToUpperCase(const nsAString& aSource,
            nsAString& aDest)
{
  const PRUnichar *in;
  PRUnichar *out;
  PRUint32 len = NS_StringGetData(aSource, &in);
  NS_StringGetMutableData(aDest, len, &out);
  NS_ASSERTION(out, "Uh...");
  ToUpperCase(in, out, len);
}

#ifdef MOZILLA_INTERNAL_API

PRInt32
nsCaseInsensitiveStringComparator::operator()(const PRUnichar* lhs,
                                              const PRUnichar* rhs,
                                              PRUint32 lLength,
                                              PRUint32 rLength) const
{
  return (lLength == rLength) ? CaseInsensitiveCompare(lhs, rhs, lLength) :
         (lLength > rLength) ? 1 : -1;
}

PRInt32
nsCaseInsensitiveUTF8StringComparator::operator()(const char* lhs,
                                                  const char* rhs,
                                                  PRUint32 lLength,
                                                  PRUint32 rLength) const
{
  return CaseInsensitiveCompare(lhs, rhs, lLength, rLength);
}

PRInt32
nsASCIICaseInsensitiveStringComparator::operator()(const PRUnichar* lhs,
                                                   const PRUnichar* rhs,
                                                   PRUint32 lLength,
                                                   PRUint32 rLength) const
{
  if (lLength != rLength) {
    if (lLength > rLength)
      return 1;
    return -1;
  }

  while (rLength) {
    PRUnichar l = *lhs++;
    PRUnichar r = *rhs++;
    if (l != r) {
      l = ToLowerCaseASCII_inline(l);
      r = ToLowerCaseASCII_inline(r);

      if (l > r)
        return 1;
      else if (r > l)
        return -1;
    }
    rLength--;
  }

  return 0;
}

#endif // MOZILLA_INTERNAL_API

PRUnichar
ToLowerCase(PRUnichar aChar)
{
  return ToLowerCase_inline(aChar);
}

void
ToLowerCase(const PRUnichar *aIn, PRUnichar *aOut, PRUint32 aLen)
{
  for (PRUint32 i = 0; i < aLen; i++) {
    aOut[i] = ToLowerCase(aIn[i]);
  }
}

PRUnichar
ToUpperCase(PRUnichar aChar)
{
  if (IS_ASCII(aChar)) {
    if (IS_ASCII_LOWER(aChar))
      return aChar - 0x20;
    else
      return aChar;
  } else if (IS_NOCASE_CHAR(aChar)) {
    return aChar;
  }

  return gUpperMap.Map(aChar);
}

void
ToUpperCase(const PRUnichar *aIn, PRUnichar *aOut, PRUint32 aLen)
{
  for (PRUint32 i = 0; i < aLen; i++) {
    aOut[i] = ToUpperCase(aIn[i]);
  }
}

PRUnichar
ToTitleCase(PRUnichar aChar)
{
  if (IS_ASCII(aChar)) {
    return ToUpperCase(aChar);
  } else if (IS_NOCASE_CHAR(aChar)) {
    return aChar;
  }

  // First check for uppercase characters whose titlecase mapping is
  // different, like U+01F1 DZ: they must remain unchanged.
  if (0x01C0 == (aChar & 0xFFC0)) {
    for (PRUint32 i = 0; i < gUpperToTitleItems; i++) {
      if (aChar == gUpperToTitle[(i*2)+kUpperIdx]) {
        return aChar;
      }
    }
  }

  PRUnichar upper = gUpperMap.Map(aChar);

  if (0x01C0 == ( upper & 0xFFC0)) {
    for (PRUint32 i = 0 ; i < gUpperToTitleItems; i++) {
      if (upper == gUpperToTitle[(i*2)+kUpperIdx]) {
         return gUpperToTitle[(i*2)+kTitleIdx];
      }
    }
  }

  return upper;
}

PRInt32
CaseInsensitiveCompare(const PRUnichar *a,
                       const PRUnichar *b,
                       PRUint32 len)
{
  NS_ASSERTION(a && b, "Do not pass in invalid pointers!");

  if (len) {
    do {
      PRUnichar c1 = *a++;
      PRUnichar c2 = *b++;

      if (c1 != c2) {
        c1 = ToLowerCase_inline(c1);
        c2 = ToLowerCase_inline(c2);
        if (c1 != c2) {
          if (c1 < c2) {
            return -1;
          }
          return 1;
        }
      }
    } while (--len != 0);
  }
  return 0;
}

// Calculates the codepoint of the UTF8 sequence starting at aStr.  Sets aNext
// to the byte following the end of the sequence.
//
// If the sequence is invalid, or if computing the codepoint would take us off
// the end of the string (as marked by aEnd), returns -1 and does not set
// aNext.  Note that this function doesn't check that aStr < aEnd -- it assumes
// you've done that already.
static NS_ALWAYS_INLINE PRUint32
GetLowerUTF8Codepoint(const char* aStr, const char* aEnd, const char **aNext)
{
  // Convert to unsigned char so that stuffing chars into PRUint32s doesn't
  // sign extend.
  const unsigned char *str = (unsigned char*)aStr;

  if (UTF8traits::isASCII(str[0])) {
    // It's ASCII; just convert to lower-case and return it.
    *aNext = aStr + 1;
    return gASCIIToLower[*str];
  }
  if (UTF8traits::is2byte(str[0]) && NS_LIKELY(aStr + 1 < aEnd)) {
    // It's a two-byte sequence, so it looks like
    //  110XXXXX 10XXXXXX.
    // This is definitely in the BMP, so we can store straightaway into a
    // PRUint16.

    PRUint16 c;
    c  = (str[0] & 0x1F) << 6;
    c += (str[1] & 0x3F);

    if (!IS_NOCASE_CHAR(c))
      c = gLowerMap.Map(c);

    *aNext = aStr + 2;
    return c;
  }
  if (UTF8traits::is3byte(str[0]) && NS_LIKELY(aStr + 2 < aEnd)) {
    // It's a three-byte sequence, so it looks like
    //  1110XXXX 10XXXXXX 10XXXXXX.
    // This will just barely fit into 16-bits, so store into a PRUint16.

    PRUint16 c;
    c  = (str[0] & 0x0F) << 12;
    c += (str[1] & 0x3F) << 6;
    c += (str[2] & 0x3F);

    if (!IS_NOCASE_CHAR(c))
      c = gLowerMap.Map(c);

    *aNext = aStr + 3;
    return c;
  }
  if (UTF8traits::is4byte(str[0]) && NS_LIKELY(aStr + 3 < aEnd)) {
    // It's a four-byte sequence, so it looks like
    //   11110XXX 10XXXXXX 10XXXXXX 10XXXXXX.
    // Unless this is an overlong sequence, the codepoint it encodes definitely
    // isn't in the BMP, so we don't bother trying to convert it to lower-case.

    PRUint32 c;
    c  = (str[0] & 0x07) << 18;
    c += (str[1] & 0x3F) << 12;
    c += (str[2] & 0x3F) << 6;
    c += (str[3] & 0x3F);

    *aNext = aStr + 4;
    return c;
  }

  // Hm, we don't understand this sequence.
  return -1;
}

PRInt32 CaseInsensitiveCompare(const char *aLeft,
                               const char *aRight,
                               PRUint32 aLeftBytes,
                               PRUint32 aRightBytes)
{
  const char *leftEnd = aLeft + aLeftBytes;
  const char *rightEnd = aRight + aRightBytes;

  while (aLeft < leftEnd && aRight < rightEnd) {
    PRUint32 leftChar = GetLowerUTF8Codepoint(aLeft, leftEnd, &aLeft);
    if (NS_UNLIKELY(leftChar == PRUint32(-1)))
      return -1;

    PRUint32 rightChar = GetLowerUTF8Codepoint(aRight, rightEnd, &aRight);
    if (NS_UNLIKELY(rightChar == PRUint32(-1)))
      return -1;

    // Now leftChar and rightChar are lower-case, so we can compare them.
    if (leftChar != rightChar) {
      if (leftChar > rightChar)
        return 1;
      return -1;
    }
  }

  // Make sure that if one string is longer than the other we return the
  // correct result.
  if (aLeft < leftEnd)
    return 1;
  if (aRight < rightEnd)
    return -1;

  return 0;
}

bool
CaseInsensitiveUTF8CharsEqual(const char* aLeft, const char* aRight,
                              const char* aLeftEnd, const char* aRightEnd,
                              const char** aLeftNext, const char** aRightNext,
                              bool* aErr)
{
  NS_ASSERTION(aLeftNext, "Out pointer shouldn't be null.");
  NS_ASSERTION(aRightNext, "Out pointer shouldn't be null.");
  NS_ASSERTION(aErr, "Out pointer shouldn't be null.");
  NS_ASSERTION(aLeft < aLeftEnd, "aLeft must be less than aLeftEnd.");
  NS_ASSERTION(aRight < aRightEnd, "aRight must be less than aRightEnd.");

  PRUint32 leftChar = GetLowerUTF8Codepoint(aLeft, aLeftEnd, aLeftNext);
  if (NS_UNLIKELY(leftChar == PRUint32(-1))) {
    *aErr = true;
    return false;
  }

  PRUint32 rightChar = GetLowerUTF8Codepoint(aRight, aRightEnd, aRightNext);
  if (NS_UNLIKELY(rightChar == PRUint32(-1))) {
    *aErr = true;
    return false;
  }

  // Can't have an error past this point.
  *aErr = false;

  return leftChar == rightChar;
}

namespace mozilla {

PRUint32
HashUTF8AsUTF16(const char* aUTF8, PRUint32 aLength, bool* aErr)
{
  PRUint32 hash = 0;
  const char* s = aUTF8;
  const char* end = aUTF8 + aLength;

  *aErr = false;

  while (s < end)
  {
    PRUint32 ucs4 = UTF8CharEnumerator::NextChar(&s, end, aErr);
    if (*aErr) {
      return 0;
    }

    if (ucs4 < PLANE1_BASE) {
      hash = AddToHash(hash, ucs4);
    }
    else {
      hash = AddToHash(hash, H_SURROGATE(ucs4), L_SURROGATE(ucs4));
    }
  }

  return hash;
}

} // namespace mozilla
