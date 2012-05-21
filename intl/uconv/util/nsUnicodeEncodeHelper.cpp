/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pratom.h"
#include "unicpriv.h"
#include "nsIUnicodeEncoder.h"
#include "nsUnicodeEncodeHelper.h"

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [implementation]
nsresult nsUnicodeEncodeHelper::ConvertByTable(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     uScanClassID aScanClass,
                                     uShiftOutTable * aShiftOutTable,
                                     uMappingTable  * aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;

  while (src < srcEnd) {
    if (!uMapCode((uTable*) aMappingTable, static_cast<PRUnichar>(*(src++)), reinterpret_cast<PRUint16*>(&med))) {
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
                                 (PRUint8 *)dest, destLen, 
                                 (PRUint32 *)&bcw);
    } else {
      charFound = uGenerate(aScanClass, 0, med,
                            (PRUint8 *)dest, destLen, 
                            (PRUint32 *)&bcw);
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
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     uScanClassID * aScanClassArray,
                                     uShiftOutTable ** aShiftOutTable, 
                                     uMappingTable  ** aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;
  PRInt32 i;

  while (src < srcEnd) {
    for (i=0; i<aTableCount; i++) 
      if (uMapCode((uTable*) aMappingTable[i], static_cast<PRUint16>(*src), reinterpret_cast<PRUint16*>(&med))) break;

    src++;
    if (i == aTableCount) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    bool charFound;
    if (aScanClassArray[i] == uMultibytesCharset) {
      NS_ASSERTION(aShiftOutTable[i], "shift table missing");
      charFound = uGenerateShift(aShiftOutTable[i], 0, med,
                                 (PRUint8 *)dest, destLen,
                                 (PRUint32 *)&bcw);
    }
    else
      charFound = uGenerate(aScanClassArray[i], 0, med,
                            (PRUint8 *)dest, destLen, 
                            (PRUint32 *)&bcw);
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
