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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "pratom.h"
#include "unicpriv.h"
#include "nsUConvDll.h"
#include "nsIMappingCache.h"
#include "nsMappingCache.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeDecoder.h"

//----------------------------------------------------------------------
// Class nsUnicodeDecodeHelper [declaration]

/**
 * The actual implementation of the nsIUnicodeDecodeHelper interface.
 *
 * @created         18/Mar/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeDecodeHelper : public nsIUnicodeDecodeHelper
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeDecodeHelper();

  /**
   * Class destructor.
   */
  virtual ~nsUnicodeDecodeHelper();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecodeHelper [declaration]

  NS_IMETHOD ConvertByTable(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  NS_IMETHOD ConvertByMultiTable(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uRange * aRangeArray, uShiftTable ** aShiftTable, 
      uMappingTable ** aMappingTable);

  NS_IMETHOD ConvertByFastTable(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength, PRUnichar * aFastTable, 
      PRInt32 aTableSize);

  NS_IMETHOD CreateFastTable( uShiftTable * aShiftTable, 
      uMappingTable * aMappingTable, PRUnichar * aFastTable, 
      PRInt32 aTableSize);

  NS_IMETHOD CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult);

  NS_IMETHOD DestroyCache(nsIMappingCache aResult);
};

//----------------------------------------------------------------------
// Class nsUnicodeDecodeHelper [implementation]

NS_IMPL_ISUPPORTS1(nsUnicodeDecodeHelper, nsIUnicodeDecodeHelper)

nsUnicodeDecodeHelper::nsUnicodeDecodeHelper() 
{
  NS_INIT_REFCNT();
}

nsUnicodeDecodeHelper::~nsUnicodeDecodeHelper() 
{
}

//----------------------------------------------------------------------
// Interface nsIUnicodeDecodeHelper [implementation]

NS_IMETHODIMP nsUnicodeDecodeHelper::ConvertByTable(
                                     const char * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     uShiftTable * aShiftTable, 
                                     uMappingTable  * aMappingTable)
{
  const char * src = aSrc;
  PRInt32 srcLen = *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRUnichar med;
  PRInt32 bcr; // byte count for read
  nsresult res = NS_OK;

  while ((srcLen > 0) && (dest < destEnd)) {
    if (!uScan(aShiftTable, NULL, (PRUint8 *)src, NS_REINTERPRET_CAST(PRUint16*, &med), srcLen, 
    (PRUint32 *)&bcr)) {
      res = NS_OK_UDEC_MOREINPUT;
      break;
    }

    if (!uMapCode((uTable*) aMappingTable, NS_STATIC_CAST(PRUint16, med), NS_REINTERPRET_CAST(PRUint16*, dest))) {
      if (med < 0x20) {
        // somehow some table miss the 0x00 - 0x20 part
        *dest = med;
      } else {
        // Unicode replacement value for unmappable chars
        *dest = 0xfffd;
      }
    }

    src += bcr;
    srcLen -= bcr;
    dest++;
  }

  if ((srcLen > 0) && (res == NS_OK)) res = NS_OK_UDEC_MOREOUTPUT;

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeDecodeHelper::ConvertByMultiTable(
                                     const char * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     uRange * aRangeArray, 
                                     uShiftTable ** aShiftTable, 
                                     uMappingTable ** aMappingTable)
{
  PRUint8 * src = (PRUint8 *)aSrc;
  PRInt32 srcLen = *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRUnichar med;
  PRInt32 bcr; // byte count for read
  nsresult res = NS_OK;
  PRInt32 i;

  while ((srcLen > 0) && (dest < destEnd)) 
  {
    PRBool done= PR_FALSE;
    PRBool passRangeCheck = PR_FALSE;
    PRBool passScan = PR_FALSE;
    for (i=0; (!done) && (i<aTableCount); i++)  
    {
      if ((aRangeArray[i].min <= *src) && (*src <= aRangeArray[i].max)) 
      {
        passRangeCheck = PR_TRUE;
        if (uScan(aShiftTable[i], NULL, src, 
                   NS_REINTERPRET_CAST(PRUint16*, &med), srcLen, 
                   (PRUint32 *)&bcr)) 
        {
          passScan = PR_TRUE;
          done = uMapCode((uTable*) aMappingTable[i], 
                          NS_STATIC_CAST(PRUint16, med), 
                          NS_REINTERPRET_CAST(PRUint16*, dest)); 
        } // if (uScan ... )
      } // if Range
    } // for loop

    if(passRangeCheck && (! passScan))
    {
      res = NS_OK_UDEC_MOREINPUT;
      break;
    }
    if(! done)
    {
      bcr = 1;
      if ((PRUint8)*src < 0x20) {
        // somehow some table miss the 0x00 - 0x20 part
        *dest = *src;
      } else if(*src == (PRUint8) 0xa0) {
        // handle nbsp
        *dest = 0x00a0;
      } else {
        // we need to decide how many byte we skip. We can use uScan to do this
        for (i=0; i<aTableCount; i++)  
        {
          if ((aRangeArray[i].min <= *src) && (*src <= aRangeArray[i].max)) 
          {
            if (uScan(aShiftTable[i], NULL, src, 
                   NS_REINTERPRET_CAST(PRUint16*, &med), srcLen, 
                   (PRUint32*)&bcr)) 
            { 
               // match the patten
              
               PRInt32 k; 
               for(k = 1; k < bcr; k++)
               {
                 if(0 == (src[k]  & 0x80))
                 { // only skip if all bytes > 0x80
                   // if we hit bytes <= 0x80, skip only one byte
                   bcr = 1;
                   break; 
                 }
               }
               break;
            }
          }
        }
        // treat it as NSBR if bcr == 1 and it is 0xa0
        *dest = ((1==bcr)&&(*src == (PRUint8)0xa0 )) ? 0x00a0 : 0xfffd;
      }
    }

    src += bcr;
    srcLen -= bcr;
    dest++;
  } // while

  if ((srcLen > 0) && (res == NS_OK)) res = NS_OK_UDEC_MOREOUTPUT;

  *aSrcLength = src - (PRUint8 *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeDecodeHelper::CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult)
{
   return nsMappingCache::CreateCache(aType, aResult);
}

NS_IMETHODIMP nsUnicodeDecodeHelper::DestroyCache(nsIMappingCache aCache)
{
   return nsMappingCache::DestroyCache(aCache);
}

NS_IMETHODIMP nsUnicodeDecodeHelper::ConvertByFastTable(
                                     const char * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRUnichar * aFastTable, 
                                     PRInt32 aTableSize)
{
  PRUint8 * src = (PRUint8 *)aSrc;
  PRUint8 * srcEnd = src;
  PRUnichar * dest = aDest;

  nsresult res;
  if (*aSrcLength > *aDestLength) {
    srcEnd += (*aDestLength);
    res = NS_PARTIAL_MORE_OUTPUT;
  } else {
    srcEnd += (*aSrcLength);
    res = NS_OK;
  }

  for (; src<srcEnd;) *dest++ = aFastTable[*src++];

  *aSrcLength = src - (PRUint8 *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeDecodeHelper::CreateFastTable(
                                     uShiftTable * aShiftTable, 
                                     uMappingTable  * aMappingTable,
                                     PRUnichar * aFastTable, 
                                     PRInt32 aTableSize)
{
  PRInt32 tableSize = aTableSize;
  PRInt32 buffSize = aTableSize;
  char * buff = new char [buffSize];
  if (buff == NULL) return NS_ERROR_OUT_OF_MEMORY;

  char * p = buff;
  for (PRInt32 i=0; i<aTableSize; i++) *(p++) = i;
  nsresult res = ConvertByTable(buff, &buffSize, aFastTable, &tableSize, 
      aShiftTable, aMappingTable);

  delete [] buff;
  return res;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewUnicodeDecodeHelper(nsISupports* aOuter, 
                          const nsIID &aIID,
                          void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsUnicodeDecodeHelper* inst = new nsUnicodeDecodeHelper();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}
