/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCaseConversionImp2.h"
#include "casetable.h"


// For gUpperToTitle 
enum {
  kUpperIdx =0,
  kTitleIdx
};

// For gUpperToTitle
enum {
  kLowIdx =0,
  kHighIdx,
  kEveryIdx,
  kDiffIdx
};

#define IS_ASCII(u)       ( 0x0000 == ((u) & 0xFF80))
#define IS_ASCII_UPPER(u) ((0x0041 <= (u)) && ( (u) <= 0x005a))
#define IS_ASCII_LOWER(u) ((0x0061 <= (u)) && ( (u) <= 0x007a))
#define IS_ASCII_ALPHA(u) (IS_ASCII_UPPER(u) || IS_ASCII_LOWER(u))

#define IS_NOCASE_CHAR(u) (PR_FALSE)
// consider the following range need double check
//  0x0600 0x0FFF
//  0x1100 0x1DFF
//  0x2000 0x20FF
//  0x2200 0x23FF
//  0x2500 0xFEFF

// Size of Tables


#define CASE_MAP_CACHE_SIZE 0x40
#define CASE_MAP_CACHE_MASK 0x3F

class nsCompressedMap {
public:
   nsCompressedMap(PRUnichar *aTable, PRUint32 aSize);
   ~nsCompressedMap();
   PRUnichar Map(PRUnichar aChar);
protected:
   PRUnichar Lookup(PRUint32 l, PRUint32 m, PRUint32 r, PRUnichar aChar);

private: 
   PRUnichar *mTable;
   PRUint32 mSize;
   PRUint32 *mCache;
};

nsCompressedMap::nsCompressedMap(PRUnichar *aTable, PRUint32 aSize)
{
   mTable = aTable;
   mSize = aSize;
   mCache = new PRUint32[CASE_MAP_CACHE_SIZE];
   for(int i = 0; i < CASE_MAP_CACHE_SIZE; i++)
      mCache[i] = 0;
}

nsCompressedMap::~nsCompressedMap()
{
   delete mCache;
}

PRUnichar nsCompressedMap::Map(PRUnichar aChar)
{
   // no need to worry thread since cached value are 
   // not object but primitive data type which could be 
   // accessed in atomic operation. We need to access
   // the whole 32 bit of cachedData at once in order to make it
   // thread safe. Never access bits from mCache dirrectly

   PRUint32 cachedData = mCache[aChar & CASE_MAP_CACHE_MASK];
   if(aChar == ((cachedData >> 16) & 0x0000FFFF))
     return (cachedData & 0x0000FFFF);

   PRUnichar res = this->Lookup(0, (mSize/2), mSize-1, aChar);

   mCache[aChar & CASE_MAP_CACHE_MASK] =
       (((aChar << 16) & 0xFFFF0000) | (0x0000FFFF & res));
   return res;
}

PRUnichar nsCompressedMap::Lookup(
   PRUint32 l, PRUint32 m, PRUint32 r, PRUnichar aChar)
{
  if ( aChar >  mTable[(m<<2)+kHighIdx]  ) {
    if( l > m )
      return aChar;
    PRUint32 newm = (m+r+1)/2;
    if(newm == m)
	   newm++;
    return this->Lookup(m+1, newm , r, aChar);
    
  } else if ( mTable[(m<<2)+kLowIdx]  > aChar ) {
    if( r < m )
      return aChar;
    PRUint32 newm = (l+m-1)/2;
    if(newm == m)
	   newm++;
	return this->Lookup(l, newm, m-1, aChar);

  } else  {
    if((mTable[(m<<2)+kEveryIdx] > 0) && 
       (0 != ((aChar - mTable[(m<<2)+kLowIdx]) % mTable[(m<<2)+kEveryIdx])))
    {
       return aChar;
    }
    return aChar + mTable[(m<<2)+kDiffIdx];
  }
}

NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);

PRBool nsCaseConversionImp2::gInit      = PR_FALSE;

NS_IMPL_ISUPPORTS(nsCaseConversionImp2, kCaseConversionIID);

static nsCompressedMap *gUpperMap = nsnull;
static nsCompressedMap *gLowerMap = nsnull;

void nsCaseConversionImp2::Init()
{
  gUpperMap = new nsCompressedMap(&gToUpper[0], gToUpperItems);
  gLowerMap = new nsCompressedMap(&gToLower[0], gToLowerItems);
  gInit = PR_TRUE;
}
nsresult nsCaseConversionImp2::ToUpper(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  if( IS_ASCII(aChar)) // optimize for ASCII
  {
     if(IS_ASCII_LOWER(aChar))
        *aReturn = aChar - 0x0020;
     else
        *aReturn = aChar;
  } 
  else if( IS_NOCASE_CHAR(aChar)) // optimize for block which have no case
  {
    *aReturn = aChar;
  } 
  else 
  {
    *aReturn = gUpperMap->Map(aChar);
  }
  return NS_OK;
}

nsresult nsCaseConversionImp2::ToLower(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  if( IS_ASCII(aChar)) // optimize for ASCII
  {
     if(IS_ASCII_UPPER(aChar))
        *aReturn = aChar + 0x0020;
     else
        *aReturn = aChar;
  } 
  else if( IS_NOCASE_CHAR(aChar)) // optimize for block which have no case
  {
    *aReturn = aChar;
  } 
  else
  {
    *aReturn = gLowerMap->Map(aChar);
  } 
  return NS_OK;
}

nsresult nsCaseConversionImp2::ToTitle(
  PRUnichar aChar, PRUnichar* aReturn
)
{
  if( IS_ASCII(aChar)) // optimize for ASCII
  {
    return this->ToUpper(aChar, aReturn);
  }
  else if( IS_NOCASE_CHAR(aChar)) // optimize for block which have no case
  {
    *aReturn = aChar;
  } 
  else
  {
    PRUnichar upper;
    upper = gUpperMap->Map(aChar);
    for(PRUint32 i = 0 ; i < gUpperToTitleItems; i++) {
      if ( upper == gUpperToTitle[(i<<1)+kUpperIdx]) {
         *aReturn = gUpperToTitle[(i<<1)+kTitleIdx];
         return NS_OK;
      }
    }
    *aReturn = upper;
  }
  return NS_OK;
}

nsresult nsCaseConversionImp2::ToUpper(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen
)
{
  PRUint32 i;
  for(i=0;i<aLen;i++) 
  {
    PRUnichar aChar = anArray[i];
    if( IS_ASCII(aChar)) // optimize for ASCII
    {
       if(IS_ASCII_LOWER(aChar))
          aReturn[i] = aChar - 0x0020;
       else
          aReturn[i] = aChar;
    }
    else if( IS_NOCASE_CHAR(aChar)) // optimize for block which have no case
    {
          aReturn[i] = aChar;
    } 
    else 
    {
      aReturn[i] = gUpperMap->Map(aChar);
    }
  }
  return NS_OK;
}

nsresult nsCaseConversionImp2::ToLower(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen
)
{
  PRUint32 i;
  for(i=0;i<aLen;i++) 
  {
    PRUnichar aChar = anArray[i];
    if( IS_ASCII(aChar)) // optimize for ASCII
    {
       if(IS_ASCII_UPPER(aChar))
          aReturn[i] = aChar + 0x0020;
       else
          aReturn[i] = aChar;
    } 
    else if( IS_NOCASE_CHAR(aChar)) // optimize for block which have no case
    {
          aReturn[i] = aChar;
    } 
    else 
    {
      aReturn[i] = gLowerMap->Map(aChar);
    }
  }
  return NS_OK;
}



nsresult nsCaseConversionImp2::ToTitle(
  const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen,
  PRBool aStartInWordBoundary
)
{
  if(0 == aLen)
    return NS_OK;

  //
  // We need to replace this implementation to a real one
  // Currently, it only do the right thing for ASCII
  // Howerver, we need a word breaker to do the right job
  //
  this->ToLower(anArray, aReturn, aLen);
  PRBool bLastIsChar = bLastIsChar =  IS_ASCII_LOWER(anArray[0]);
  if(aStartInWordBoundary)
  {
     this->ToTitle(aReturn[0], &aReturn[0]);
  }

  PRUint32 i;
  for(i=1;i<aLen;i++)
  {
    if(!bLastIsChar)
    {
      this->ToTitle(aReturn[i], &aReturn[i]);
    }

    bLastIsChar = IS_ASCII_LOWER(aReturn[i]);
  }
  return NS_OK;
}
