/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "pratom.h"
#include "nsUConvDll.h"
#include "nsIComponentManager.h"
#include "nsUnicodeDecodeUtil.h"
#include "nsIUnicodeDecoder.h"


// #define NO_OPTIMIZATION_FOR_1TABLE
#define UCONV_ASSERT(a)

NS_IMPL_ISUPPORTS( nsUnicodeDecodeUtil, kIUnicodeDecodeUtilIID);

nsUnicodeDecodeUtil::nsUnicodeDecodeUtil()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
nsUnicodeDecodeUtil::~nsUnicodeDecodeUtil()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

#ifdef NO_OPTIMIZATION_FOR_1TABLE
NS_IMETHODIMP nsUnicodeDecodeUtil::Convert(
    PRUnichar      *aDest,
    PRInt32         aDestOffset,
    PRInt32        *aDestLength,
    const char     *aSrc,
    PRInt32         aSrcOffset,
    PRInt32        *aSrcLength,
    PRInt32         aBehavior,
    uShiftTable    *aShiftTable,
    uMappingTable  *aMappingTable
)
{
   uRange range = {0x00, 0xff};
   return Convert(aDest, aDestOffset, aDestLength, 
                  aSrc, aSrcOffset, aSrcLength,
	          aBehavior, 1, &range,
	          aShiftTable, aMappingTable);
}
#else
NS_IMETHODIMP nsUnicodeDecodeUtil::Convert(
    PRUnichar      *aDest,
    PRInt32         aDestOffset,
    PRInt32        *aDestLength,
    const char     *aSrc,
    PRInt32         aSrcOffset,
    PRInt32        *aSrcLength,
    PRInt32         aBehavior,
    uShiftTable    *aShiftTable,
    uMappingTable  *aMappingTable
)
{
  PRUint32 ubuflen = *aDestLength;
  PRUint32 srclen = *aSrcLength;
  PRUint32 validlen,scanlen;
  PRUint16 med;
  unsigned char* src = (unsigned char*)aSrc + aSrcOffset;
  PRUnichar* dest = aDest + aDestOffset;

  *dest = (PRUnichar) 0;
  for(validlen=0; 
          ((srclen > 0) &&  (ubuflen > 0));
              srclen -= scanlen, src += scanlen, dest++, ubuflen--,validlen++ ) {
      scanlen = 0;
      if(uScan(aShiftTable,	(PRInt32*) 0, src, &med, srclen, &scanlen))	{
          uMapCode((uTable*) aMappingTable,med, dest);
          if(*dest == NOMAPPING) {
            if((*src < 0x20)  || (0x7F == *src))
            { // somehow some table miss the 0x00 - 0x20 part 
              *dest = (PRUnichar) *src;
            } else if(nsIUnicodeDecoder::kOnError_Signal == aBehavior)
            {
              *aSrcLength -= srclen;
              *aDestLength = validlen;
              return NS_ERROR_ILLEGAL_INPUT;
            }
            if(scanlen == 0)
              scanlen = 1;
      	  }
      }	else {
          *aSrcLength -= srclen;
          *aDestLength = validlen;
          return NS_PARTIAL_MORE_INPUT;
      }
  }
  *aSrcLength -= srclen;
  *aDestLength = validlen;
  if(srclen > 0)
	return NS_PARTIAL_MORE_OUTPUT;
  else 
    return NS_OK;
}
#endif

NS_IMETHODIMP nsUnicodeDecodeUtil::Convert(
    PRUnichar      *aDest,
    PRInt32         aDestOffset,
    PRInt32        *aDestLength,
    const char     *aSrc,
    PRInt32         aSrcOffset,
    PRInt32        *aSrcLength,
    PRInt32         aBehavior,
    PRUint16        numberOfTable,
    uRange         *aRangeArray,
    uShiftTable    ** aShiftTableArray,
    uMappingTable  ** aMappingTableArray
)
{
  PRUint32 ubuflen = *aDestLength;
  PRUint32 srclen = *aSrcLength;
  PRUint32 validlen,scanlen;
  PRUint16 med;
  unsigned char* src = (unsigned char*)aSrc + aSrcOffset;
  PRUnichar* dest = aDest + aDestOffset;

  *dest = (PRUnichar) 0;
  for(validlen=0;  
          ((srclen > 0) && (ubuflen > 0));
              srclen -= scanlen, src += scanlen, dest++, ubuflen--,validlen++ ) {
      PRUint16 i;
      scanlen = 0;
      for(i=0;i<numberOfTable;i++) {
        if((aRangeArray[i].min  <= *src) &&  
           (*src <= aRangeArray[i].max)) {  
          if(uScan(aShiftTableArray[i], (PRInt32*) 0,
		          src, &med, srclen, &scanlen)) {
            uMapCode((uTable*) aMappingTableArray[i],med, dest);
            if(*dest != NOMAPPING)
              break;
		  } else {
              *aSrcLength -= srclen;
              *aDestLength = validlen;
              return NS_PARTIAL_MORE_INPUT;
		  }
        }
      }
      if(i == numberOfTable) {
         if((*src < 0x20)  || (0x7F == *src))
         { // somehow some table miss the 0x00 - 0x20 part 
           *dest = (PRUnichar) *src;
         } else  if(nsIUnicodeDecoder::kOnError_Signal == aBehavior)
         {
           *aSrcLength -= srclen;
           *aDestLength = validlen;
           return NS_ERROR_ILLEGAL_INPUT;
         } else {
           *dest= NOMAPPING;
         }
         if(scanlen == 0)
            scanlen = 1;
      }
  }
  *aSrcLength -= srclen;
  *aDestLength = validlen;
  if(srclen > 0)
	return NS_PARTIAL_MORE_OUTPUT;
  else 
    return NS_OK;
}


NS_IMETHODIMP nsUnicodeDecodeUtil::Init1ByteFastTable(
   uMappingTable   *aMappingTable,
   PRUnichar       *aFastTable
)
{
   static PRInt16 g1ByteShiftTable[] = {
           0, u1ByteCharset, 
           ShiftCell(0, 0,0,0,0,0,0,0),
   };
   static char dmy[256];
   static PRBool init=PR_FALSE;
   if(! init)
   {
      for(int i= 0;i < 256; i++)
         dmy[i] = (char) i;
      init = PR_TRUE;
   }
   PRInt32 dm1 = 256;
   PRInt32 dm2 = 256;
   return Convert(aFastTable, 0, &dm1, dmy, 0, &dm2,  
                  nsIUnicodeDecoder::kOnError_Recover,
                  (uShiftTable*) &g1ByteShiftTable, aMappingTable);
}

NS_IMETHODIMP nsUnicodeDecodeUtil::Convert(
   PRUnichar       *aDest,
   PRInt32          aDestOffset,
   PRInt32         *aDestLength,
   const char      *aSrc,
   PRInt32          aSrcOffset,
   PRInt32         *aSrcLength,
   const PRUnichar *aFastTable
)
{
   PRUnichar *pD = aDest + aDestOffset;
   unsigned char *pS = (unsigned char*) (aSrc + aSrcOffset);	
   PRInt32 srclen = *aSrcLength;
   PRInt32 destlen = *aDestLength;

   for( ; ((srclen > 0) && ( destlen > 0)); srclen--, destlen--)
     *pD++ = aFastTable[*pS++];

   *aSrcLength -= srclen;
   *aDestLength -= destlen;

   if(srclen > 0)
	return NS_PARTIAL_MORE_OUTPUT;
   else 
        return NS_OK;
}
