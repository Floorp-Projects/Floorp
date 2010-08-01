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

#define IS_ASCII(u)       ( 0x0000 == ((u) & 0xFF80))
#define IS_ASCII_UPPER(u) ((0x0041 <= (u)) && ( (u) <= 0x005a))
#define IS_ASCII_LOWER(u) ((0x0061 <= (u)) && ( (u) <= 0x007a))
#define IS_ASCII_ALPHA(u) (IS_ASCII_UPPER(u) || IS_ASCII_LOWER(u))
#define IS_ASCII_SPACE(u) ( 0x0020 == (u) )

#define IS_NOCASE_CHAR(u)  (0==(1&(gCaseBlocks[(u)>>13]>>(0x001F&((u)>>8)))))
  
// Size of Tables

#define CASE_MAP_CACHE_SIZE 0x40
#define CASE_MAP_CACHE_MASK 0x3F

struct nsCompressedMap {
  const PRUnichar *mTable;
  PRUint32 mSize;
  PRUint32 mCache[CASE_MAP_CACHE_SIZE];
  PRUint32 mLastBase;

  PRUnichar Map(PRUnichar aChar)
  {
    // no need to worry about thread safety since cached values are
    // not objects but primitive data types which could be 
    // accessed in atomic operations. We need to access
    // the whole 32 bit of cachedData at once in order to make it
    // thread safe. Never access bits from mCache directly.

    PRUint32 cachedData = mCache[aChar & CASE_MAP_CACHE_MASK];
    if(aChar == ((cachedData >> 16) & 0x0000FFFF))
      return (cachedData & 0x0000FFFF);

    // try the last index first
    // store into local variable so we can be thread safe
    PRUint32 base = mLastBase; 
    PRUnichar res = 0;
    
    if (( aChar <=  ((mTable[base+kSizeEveryIdx] >> 8) + 
                  mTable[base+kLowIdx])) &&
        ( mTable[base+kLowIdx]  <= aChar )) 
    {
        // Hit the last base
        if(((mTable[base+kSizeEveryIdx] & 0x00FF) > 0) && 
          (0 != ((aChar - mTable[base+kLowIdx]) % 
                (mTable[base+kSizeEveryIdx] & 0x00FF))))
        {
          res = aChar;
        } else {
          res = aChar + mTable[base+kDiffIdx];
        }
    } else {
        res = this->Lookup(0, (mSize/2), mSize-1, aChar);
    }

    mCache[aChar & CASE_MAP_CACHE_MASK] =
        (((aChar << 16) & 0xFFFF0000) | (0x0000FFFF & res));
    return res;
  }

  PRUnichar Lookup(PRUint32 l,
                   PRUint32 m,
                   PRUint32 r,
                   PRUnichar aChar)
  {
    PRUint32 base = m*3;
    if ( aChar >  ((mTable[base+kSizeEveryIdx] >> 8) + 
                  mTable[base+kLowIdx])) 
    {
      if( l > m )
        return aChar;
      PRUint32 newm = (m+r+1)/2;
      if(newm == m)
        newm++;
      return this->Lookup(m+1, newm , r, aChar);
      
    } else if ( mTable[base+kLowIdx]  > aChar ) {
      if( r < m )
        return aChar;
      PRUint32 newm = (l+m-1)/2;
      if(newm == m)
        newm++;
      return this->Lookup(l, newm, m-1, aChar);

    } else  {
      if(((mTable[base+kSizeEveryIdx] & 0x00FF) > 0) && 
        (0 != ((aChar - mTable[base+kLowIdx]) % 
                (mTable[base+kSizeEveryIdx] & 0x00FF))))
      {
        return aChar;
      }
      mLastBase = base; // cache the base
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
                                              PRUint32 aLength) const
{
  return CaseInsensitiveCompare(lhs, rhs, aLength);
}

PRInt32
nsCaseInsensitiveStringComparator::operator()(PRUnichar lhs,
                                              PRUnichar rhs) const
{
  // see if they're an exact match first
  if (lhs == rhs)
    return 0;
  
  lhs = ToLowerCase(lhs);
  rhs = ToLowerCase(rhs);
  
  if (lhs == rhs)
    return 0;
  else if (lhs < rhs)
    return -1;
  else
    return 1;
}

#endif // MOZILLA_INTERNAL_API

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
        c1 = ToLowerCase(c1);
        c2 = ToLowerCase(c2);
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

PRUnichar
ToLowerCase(PRUnichar aChar)
{
  if (IS_ASCII(aChar)) {
    if (IS_ASCII_UPPER(aChar))
      return aChar + 0x0020;
    else
      return aChar;
  } else if (IS_NOCASE_CHAR(aChar)) {
     return aChar;
  }

  return gLowerMap.Map(aChar);
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
      return aChar - 0x0020;
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
