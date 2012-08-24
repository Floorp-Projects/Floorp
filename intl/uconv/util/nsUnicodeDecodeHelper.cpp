/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pratom.h"
#include "unicpriv.h"
#include "nsIUnicodeDecoder.h"
#include "nsUnicodeDecodeHelper.h"
#include "nsAutoPtr.h"

//----------------------------------------------------------------------
// Class nsUnicodeDecodeHelper [implementation]
nsresult nsUnicodeDecodeHelper::ConvertByTable(
                                     const char * aSrc, 
                                     int32_t * aSrcLength, 
                                     PRUnichar * aDest, 
                                     int32_t * aDestLength, 
                                     uScanClassID aScanClass,
                                     uShiftInTable * aShiftInTable, 
                                     uMappingTable  * aMappingTable,
                                     bool aErrorSignal)
{
  const char * src = aSrc;
  int32_t srcLen = *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRUnichar med;
  int32_t bcr; // byte count for read
  nsresult res = NS_OK;

  while ((srcLen > 0) && (dest < destEnd)) {
    bool charFound;
    if (aScanClass == uMultibytesCharset) {
      NS_ASSERTION(aShiftInTable, "shift table missing");
      charFound = uScanShift(aShiftInTable, nullptr, (uint8_t *)src,
                             reinterpret_cast<uint16_t*>(&med), srcLen, 
                             (uint32_t *)&bcr);
    } else {
      charFound = uScan(aScanClass, nullptr, (uint8_t *)src,
                        reinterpret_cast<uint16_t*>(&med),
                        srcLen, (uint32_t *)&bcr);
    }
    if (!charFound) {
      res = NS_OK_UDEC_MOREINPUT;
      break;
    }

    if (!uMapCode((uTable*) aMappingTable, static_cast<uint16_t>(med), reinterpret_cast<uint16_t*>(dest))) {
      if (med < 0x20) {
        // somehow some table miss the 0x00 - 0x20 part
        *dest = med;
      } else {
        if (aErrorSignal) {
          res = NS_ERROR_ILLEGAL_INPUT;
          break;
        }
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

nsresult nsUnicodeDecodeHelper::ConvertByMultiTable(
                                     const char * aSrc, 
                                     int32_t * aSrcLength, 
                                     PRUnichar * aDest, 
                                     int32_t * aDestLength, 
                                     int32_t aTableCount, 
                                     const uRange * aRangeArray, 
                                     uScanClassID * aScanClassArray,
                                     uMappingTable ** aMappingTable,
                                     bool aErrorSignal)
{
  uint8_t * src = (uint8_t *)aSrc;
  int32_t srcLen = *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRUnichar med;
  int32_t bcr; // byte count for read
  nsresult res = NS_OK;
  int32_t i;

  while ((srcLen > 0) && (dest < destEnd)) 
  {
    bool done= false;
    bool passRangeCheck = false;
    bool passScan = false;
    for (i=0; (!done) && (i<aTableCount); i++)  
    {
      if ((aRangeArray[i].min <= *src) && (*src <= aRangeArray[i].max)) 
      {
        passRangeCheck = true;
        if (uScan(aScanClassArray[i], nullptr, src, 
                   reinterpret_cast<uint16_t*>(&med), srcLen, 
                   (uint32_t *)&bcr)) 
        {
          passScan = true;
          done = uMapCode((uTable*) aMappingTable[i], 
                          static_cast<uint16_t>(med), 
                          reinterpret_cast<uint16_t*>(dest)); 
        } // if (uScan ... )
      } // if Range
    } // for loop

    if(passRangeCheck && (! passScan))
    {
      if (res != NS_ERROR_ILLEGAL_INPUT)
        res = NS_OK_UDEC_MOREINPUT;
      break;
    }
    if(! done)
    {
      bcr = 1;
      if ((uint8_t)*src < 0x20) {
        // somehow some table miss the 0x00 - 0x20 part
        *dest = *src;
      } else if(*src == (uint8_t) 0xa0) {
        // handle nbsp
        *dest = 0x00a0;
      } else {
        // we need to decide how many byte we skip. We can use uScan to do this
        for (i=0; i<aTableCount; i++)  
        {
          if ((aRangeArray[i].min <= *src) && (*src <= aRangeArray[i].max)) 
          {
            if (uScan(aScanClassArray[i], nullptr, src, 
                   reinterpret_cast<uint16_t*>(&med), srcLen, 
                   (uint32_t*)&bcr)) 
            { 
               // match the patten
              
               int32_t k; 
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
        if ((1==bcr)&&(*src == (uint8_t)0xa0 )) {
          *dest = 0x00a0;
        } else {
          if (aErrorSignal) {
            res = NS_ERROR_ILLEGAL_INPUT;
            break;
          }
          *dest = 0xfffd;
        }
      }
    }

    src += bcr;
    srcLen -= bcr;
    dest++;
  } // while

  if ((srcLen > 0) && (res == NS_OK)) res = NS_OK_UDEC_MOREOUTPUT;

  *aSrcLength = src - (uint8_t *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsUnicodeDecodeHelper::ConvertByFastTable(
                                     const char * aSrc, 
                                     int32_t * aSrcLength, 
                                     PRUnichar * aDest, 
                                     int32_t * aDestLength, 
                                     const PRUnichar * aFastTable, 
                                     int32_t aTableSize,
                                     bool aErrorSignal)
{
  uint8_t * src = (uint8_t *)aSrc;
  uint8_t * srcEnd = src;
  PRUnichar * dest = aDest;

  nsresult res;
  if (*aSrcLength > *aDestLength) {
    srcEnd += (*aDestLength);
    res = NS_PARTIAL_MORE_OUTPUT;
  } else {
    srcEnd += (*aSrcLength);
    res = NS_OK;
  }

  for (; src<srcEnd;) {
    *dest = aFastTable[*src];
    if (*dest == 0xfffd && aErrorSignal) {
      res = NS_ERROR_ILLEGAL_INPUT;
      break;
    }
    src++;
    dest++;
  }

  *aSrcLength = src - (uint8_t *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsUnicodeDecodeHelper::CreateFastTable(
                                     uMappingTable  * aMappingTable,
                                     PRUnichar * aFastTable, 
                                     int32_t aTableSize)
{
  int32_t tableSize = aTableSize;
  int32_t buffSize = aTableSize;
  nsAutoArrayPtr<char> buff(new char [buffSize]);

  char * p = buff;
  for (int32_t i=0; i<aTableSize; i++) *(p++) = i;
  return ConvertByTable(buff, &buffSize, aFastTable, &tableSize, 
                        u1ByteCharset, nullptr, aMappingTable);
}

