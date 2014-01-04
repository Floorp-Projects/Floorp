/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "unicpriv.h"
#include "nsUnicodeEncodeHelper.h"
#include "nsDebug.h"

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [implementation]
nsresult nsUnicodeEncodeHelper::ConvertByTable(
                                     const char16_t * aSrc, 
                                     int32_t * aSrcLength, 
                                     char * aDest, 
                                     int32_t * aDestLength, 
                                     uScanClassID aScanClass,
                                     uShiftOutTable * aShiftOutTable,
                                     uMappingTable  * aMappingTable)
{
  const char16_t * src = aSrc;
  const char16_t * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  int32_t destLen = *aDestLength;

  char16_t med;
  int32_t bcw; // byte count for write;
  nsresult res = NS_OK;

  while (src < srcEnd) {
    if (!uMapCode((uTable*) aMappingTable, static_cast<char16_t>(*(src++)), reinterpret_cast<uint16_t*>(&med))) {
      if (aScanClass == u1ByteCharset && *(src - 1) < 0x20) {
        // some tables are missing the 0x00 - 0x20 part
        med = *(src - 1);
      } else {
        res = NS_ERROR_UENC_NOMAPPING;
        break;
      }
    }

    bool charFound;
    if (aScanClass == uMultibytesCharset) {
      NS_ASSERTION(aShiftOutTable, "shift table missing");
      charFound = uGenerateShift(aShiftOutTable, 0, med,
                                 (uint8_t *)dest, destLen, 
                                 (uint32_t *)&bcw);
    } else {
      charFound = uGenerate(aScanClass, 0, med,
                            (uint8_t *)dest, destLen, 
                            (uint32_t *)&bcw);
    }
    if (!charFound) {
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsUnicodeEncodeHelper::ConvertByMultiTable(
                                     const char16_t * aSrc, 
                                     int32_t * aSrcLength, 
                                     char * aDest, 
                                     int32_t * aDestLength, 
                                     int32_t aTableCount, 
                                     uScanClassID * aScanClassArray,
                                     uShiftOutTable ** aShiftOutTable, 
                                     uMappingTable  ** aMappingTable)
{
  const char16_t * src = aSrc;
  const char16_t * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  int32_t destLen = *aDestLength;

  char16_t med;
  int32_t bcw; // byte count for write;
  nsresult res = NS_OK;
  int32_t i;

  while (src < srcEnd) {
    for (i=0; i<aTableCount; i++) 
      if (uMapCode((uTable*) aMappingTable[i], static_cast<uint16_t>(*src), reinterpret_cast<uint16_t*>(&med))) break;

    src++;
    if (i == aTableCount) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    bool charFound;
    if (aScanClassArray[i] == uMultibytesCharset) {
      NS_ASSERTION(aShiftOutTable[i], "shift table missing");
      charFound = uGenerateShift(aShiftOutTable[i], 0, med,
                                 (uint8_t *)dest, destLen,
                                 (uint32_t *)&bcw);
    }
    else
      charFound = uGenerate(aScanClassArray[i], 0, med,
                            (uint8_t *)dest, destLen, 
                            (uint32_t *)&bcw);
    if (!charFound) { 
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}
