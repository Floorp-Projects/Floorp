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

#include "nsIComponentManager.h"
#include "nsISO2022JPToUnicode.h"
#include "nsUCVJA2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// XXX renames
static PRInt16 cs0201ShiftTable[] =  {
  2, u1ByteCharset ,
  ShiftCell(u1ByteChar,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
  ShiftCell(u1ByteChar,   1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
};

static PRInt16 cs0208ShiftTable[] =  {
  0, u2BytesCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

//----------------------------------------------------------------------
// Class nsISO2022JPToUnicode [implementation]

nsISO2022JPToUnicode::nsISO2022JPToUnicode() 
: nsBufferDecoderSupport()
{
  mHelper = nsnull;

  Reset();
}

nsISO2022JPToUnicode::~nsISO2022JPToUnicode() 
{
  NS_IF_RELEASE(mHelper);
}

nsresult nsISO2022JPToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsISO2022JPToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

nsresult nsISO2022JPToUnicode::ConvertBuffer(const char ** aSrc, 
                                             const char * aSrcEnd,
                                             PRUnichar ** aDest, 
                                             PRUnichar * aDestEnd)
{
  // XXX redo the logic here - are we using pointers or length ints?
  const char * src = *aSrc;
  PRUnichar * dest = *aDest;
  PRUnichar val;
  PRInt32 srcLen = aSrcEnd - src;
  PRInt32 destLen = aDestEnd - dest;
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeDecodeHelperCID, NULL, 
        kIUnicodeDecodeHelperIID, (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UDEC_NOHELPER;
  }

  if (mCharset == kASCII) {
    // single byte
    if (srcLen > destLen) {
      aSrcEnd = src + destLen;
      res = NS_PARTIAL_MORE_OUTPUT;
    } else res = NS_OK;
    // XXX use a table here too.
    for (;src<aSrcEnd;) {
      val = ((PRUint8)*src++);
      *dest++ = (val < 0x80)?val:0xfffd;
    }
  } else if (mCharset == kJISX0201_1976) {
    // single byte
    if (srcLen > destLen) {
      aSrcEnd = src + destLen;
      res = NS_PARTIAL_MORE_OUTPUT;
    } else res = NS_OK;

    res = mHelper->ConvertByTable(src, &srcLen, dest, &destLen, 
        (uShiftTable*) &cs0201ShiftTable, (uMappingTable*)&g_ut0201Mapping);
    *aSrc = src + srcLen;
    *aDest = dest + destLen;
    return res;
  } else {
    // XXX hello, use different tables for JIS X 0208-1978 and 1983 !!!
    // double byte
    if (srcLen > 2*destLen) {
      aSrcEnd = src + 2*destLen;
      res = NS_PARTIAL_MORE_OUTPUT;
    } else if (srcLen%2) {
      aSrcEnd--;
      res = NS_PARTIAL_MORE_INPUT;
    } else res = NS_OK;

    res = mHelper->ConvertByTable(src, &srcLen, dest, &destLen, 
        (uShiftTable*) &cs0208ShiftTable, (uMappingTable*)&g_ut0208Mapping);
    *aSrc = src + srcLen;
    *aDest = dest + destLen;
    return res;
  }

  *aSrc = src;
  *aDest = dest;
  return res;
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsISO2022JPToUnicode::ConvertNoBuff(const char * aSrc, 
                                                  PRInt32 * aSrcLength, 
                                                  PRUnichar * aDest, 
                                                  PRInt32 * aDestLength)
{
  const char * srcEnd = aSrc + (*aSrcLength);
  const char * src = aSrc;
  PRUnichar * destEnd = aDest + (*aDestLength);
  PRUnichar * dest = aDest;
  const char * p;
  nsresult res = NS_OK;

  for (; ((src < srcEnd) && (res == NS_OK)); src++) {
    switch (mState) {

      case 0:
        if (*src == 27) {
          mState = 1;
        } else if (dest >= destEnd) {
          res = NS_PARTIAL_MORE_OUTPUT;
          src--; // don't advance!
        } else {
          // here: src < srcEnd, dest < destEnd
          // find the end of this run
          for (p=src; ((p < srcEnd) && (*p != 27)); p++) {}
          res = ConvertBuffer(&src, p, &dest, destEnd);
          src--; // we did our own advance here
        }
        break;

      case 1:
        switch (*src) {
          case '(':
            mState = 2;
            break;
          case '$':
            mState = 3;
            break;
          default:
            res = NS_ERROR_ILLEGAL_INPUT;
        }
        break;

      case 2:
        switch (*src) {
          case 'B':
            mState = 0;
            mCharset = kASCII;
            break;
          case 'J':
            mState = 0;
            mCharset = kJISX0201_1976;
            break;
          default:
            res = NS_ERROR_ILLEGAL_INPUT;
        }
        break;

      case 3:
        switch (*src) {
          case '@':
            mState = 0;
            mCharset = kJISX0208_1978;
            break;
          case 'B':
            mState = 0;
            mCharset = kJISX0208_1983;
            break;
          default:
            res = NS_ERROR_ILLEGAL_INPUT;
        }
        break;

      default:
        res = NS_ERROR_UNEXPECTED;
    }
  }

  if ((res == NS_OK) && (mState != 0)) res = NS_PARTIAL_MORE_INPUT;

  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return res;
}

NS_IMETHODIMP nsISO2022JPToUnicode::GetMaxLength(const char * aSrc, 
                                                 PRInt32 aSrcLength, 
                                                 PRInt32 * aDestLength)
{
  // worst case
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsISO2022JPToUnicode::Reset()
{
  mState = 0;
  mCharset = kASCII;
  return nsBufferDecoderSupport::Reset();
}
