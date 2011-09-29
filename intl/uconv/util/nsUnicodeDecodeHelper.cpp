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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "pratom.h"
#include "unicpriv.h"
#include "nsIUnicodeDecoder.h"
#include "nsUnicodeDecodeHelper.h"

//----------------------------------------------------------------------
// Class nsUnicodeDecodeHelper [implementation]
nsresult nsUnicodeDecodeHelper::ConvertByTable(
                                     const char * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     uScanClassID aScanClass,
                                     uShiftInTable * aShiftInTable, 
                                     uMappingTable  * aMappingTable,
                                     bool aErrorSignal)
{
  const char * src = aSrc;
  PRInt32 srcLen = *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRUnichar med;
  PRInt32 bcr; // byte count for read
  nsresult res = NS_OK;

  while ((srcLen > 0) && (dest < destEnd)) {
    bool charFound;
    if (aScanClass == uMultibytesCharset) {
      NS_ASSERTION(aShiftInTable, "shift table missing");
      charFound = uScanShift(aShiftInTable, NULL, (PRUint8 *)src,
                             reinterpret_cast<PRUint16*>(&med), srcLen, 
                             (PRUint32 *)&bcr);
    } else {
      charFound = uScan(aScanClass, NULL, (PRUint8 *)src,
                        reinterpret_cast<PRUint16*>(&med),
                        srcLen, (PRUint32 *)&bcr);
    }
    if (!charFound) {
      res = NS_OK_UDEC_MOREINPUT;
      break;
    }

    if (!uMapCode((uTable*) aMappingTable, static_cast<PRUint16>(med), reinterpret_cast<PRUint16*>(dest))) {
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
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     const uRange * aRangeArray, 
                                     uScanClassID * aScanClassArray,
                                     uMappingTable ** aMappingTable,
                                     bool aErrorSignal)
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
    bool done= false;
    bool passRangeCheck = false;
    bool passScan = false;
    for (i=0; (!done) && (i<aTableCount); i++)  
    {
      if ((aRangeArray[i].min <= *src) && (*src <= aRangeArray[i].max)) 
      {
        passRangeCheck = PR_TRUE;
        if (uScan(aScanClassArray[i], NULL, src, 
                   reinterpret_cast<PRUint16*>(&med), srcLen, 
                   (PRUint32 *)&bcr)) 
        {
          passScan = PR_TRUE;
          done = uMapCode((uTable*) aMappingTable[i], 
                          static_cast<PRUint16>(med), 
                          reinterpret_cast<PRUint16*>(dest)); 
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
            if (uScan(aScanClassArray[i], NULL, src, 
                   reinterpret_cast<PRUint16*>(&med), srcLen, 
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
        if ((1==bcr)&&(*src == (PRUint8)0xa0 )) {
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

  *aSrcLength = src - (PRUint8 *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsUnicodeDecodeHelper::ConvertByFastTable(
                                     const char * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     PRUnichar * aDest, 
                                     PRInt32 * aDestLength, 
                                     const PRUnichar * aFastTable, 
                                     PRInt32 aTableSize,
                                     bool aErrorSignal)
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

  for (; src<srcEnd;) {
    *dest = aFastTable[*src];
    if (*dest == 0xfffd && aErrorSignal) {
      res = NS_ERROR_ILLEGAL_INPUT;
      break;
    }
    src++;
    dest++;
  }

  *aSrcLength = src - (PRUint8 *)aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsUnicodeDecodeHelper::CreateFastTable(
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
      u1ByteCharset, nsnull, aMappingTable);

  delete [] buff;
  return res;
}

