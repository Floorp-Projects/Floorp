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

#include "nsUnicodeToISO2022JP.h"
#include "nsIComponentManager.h"
#include "nsUCVJA2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ufAsciiMapping [] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

static PRInt16 g_ufAsciiShift [] =  { 
  0, u1ByteCharset, 
  ShiftCell(0,0,0,0,0,0,0,0) 
};

static PRInt16 g_uf0201Shift [] =  {
  2, u1ByteCharset ,
  ShiftCell(u1ByteChar,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
  ShiftCell(u1ByteChar,   1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
};

static PRInt16 g_uf0208Shift [] =  {
  0, u2BytesCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRUint16 * g_ufMappingTables[4] = {
  g_ufAsciiMapping,
  g_uf0201Mapping,
  g_uf0208Mapping,
  g_uf0208Mapping,
};

static PRInt16 * g_ufShiftTables[4] = {
  g_ufAsciiShift,
  g_uf0201Shift,
  g_uf0208Shift,
  g_uf0208Shift,
};

//----------------------------------------------------------------------
// Class nsUnicodeToISO2022JP [implementation]

nsUnicodeToISO2022JP::nsUnicodeToISO2022JP() 
: nsEncoderSupport()
{
  mHelper = NULL;
  Reset();
}

nsUnicodeToISO2022JP::~nsUnicodeToISO2022JP() 
{
  NS_IF_RELEASE(mHelper);
}

nsresult nsUnicodeToISO2022JP::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUnicodeToISO2022JP();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

nsresult nsUnicodeToISO2022JP::ChangeCharset(PRInt32 aCharset,
                                             char * aDest, 
                                             PRInt32 * aDestLength)
{
  if (aCharset == mCharset) {
    *aDestLength = 0;
    return NS_OK;
  } 
  
  if (*aDestLength < 3) {
    *aDestLength = 0;
    return NS_OK_UENC_MOREOUTPUT;
  }

  switch (aCharset) {
    case 0:
      aDest[0] = 0x1b;
      aDest[1] = '(';
      aDest[2] = 'B';
      break;
    case 1:
      aDest[0] = 0x1b;
      aDest[1] = '(';
      aDest[2] = 'J';
      break;
    case 2:
      aDest[0] = 0x1b;
      aDest[1] = '$';
      aDest[2] = '@';
      break;
    case 3:
      aDest[0] = 0x1b;
      aDest[1] = '$';
      aDest[2] = 'B';
      break;
  }

  mCharset = aCharset;
  *aDestLength = 3;
  return NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToISO2022JP::ConvertNoBuffNoErr(
                                    const PRUnichar * aSrc, 
                                    PRInt32 * aSrcLength, 
                                    char * aDest, 
                                    PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeEncodeHelperCID, NULL, 
        kIUnicodeEncodeHelperIID, (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  PRInt32 bcr, bcw;
  PRInt32 i;

  while (src < srcEnd) {
    for (i=0; i<4; i++) {
      bcr = 1;
      bcw = destEnd - dest;
      res = mHelper->ConvertByTable(src, &bcr, dest, &bcw, 
          (uShiftTable *) g_ufShiftTables[i], 
          (uMappingTable *) g_ufMappingTables[i]);
      if (res != NS_ERROR_UENC_NOMAPPING) break;
    }

    if (i>=4) res = NS_ERROR_UENC_NOMAPPING;
    if (res != NS_OK) break;

    bcw = destEnd - dest;
    res = ChangeCharset(i, dest, &bcw);
    dest += bcw;
    if (res != NS_OK) break;

    bcr = srcEnd - src;
    bcw = destEnd - dest;
    res = mHelper->ConvertByTable(src, &bcr, dest, &bcw, 
        (uShiftTable *) g_ufShiftTables[i], 
        (uMappingTable *) g_ufMappingTables[i]);
    src += bcr;
    dest += bcw;

    if ((res != NS_OK) && (res != NS_ERROR_UENC_NOMAPPING)) break;
    if (res == NS_ERROR_UENC_NOMAPPING) src--;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeToISO2022JP::FinishNoBuff(char * aDest, 
                                                 PRInt32 * aDestLength)
{
  ChangeCharset(0, aDest, aDestLength);
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToISO2022JP::GetMaxLength(const PRUnichar * aSrc, 
                                                 PRInt32 aSrcLength,
                                                 PRInt32 * aDestLength)
{
  // worst case
  *aDestLength = 5*aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToISO2022JP::Reset()
{
  mCharset = 0;
  return nsEncoderSupport::Reset();
}
