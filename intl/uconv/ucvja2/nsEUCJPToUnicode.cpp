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
#include "nsIComponentManager.h"
#include "nsEUCJPToUnicode.h"
#include "nsUCVJA2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static PRInt16 g0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static PRInt16 g0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g0212ShiftTable[] =  {
        0, u2BytesGRPrefix8EA2Charset,
        ShiftCell(0,0,0,0,0,0,0,0)
};
static PRInt16 *gShiftTables[4] =  {
    g0208ShiftTable,
    g0201ShiftTable,
    g0201ShiftTable,
    g0212ShiftTable
};

static PRUint16 *gMappingTables[4] = {
    g_0208Mapping,
    g_0201Mapping,
    g_0201Mapping,
    g_0212Mapping
};

static uRange gRanges[] = {
    { 0xA1, 0xFE },
    { 0x00, 0x7F },
    { 0x8E, 0x8E },
    { 0x8F, 0x8F } 
};

//----------------------------------------------------------------------
// Class nsEUCJPToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsEUCJPToUnicode, kIUnicodeDecoderIID);

nsEUCJPToUnicode::nsEUCJPToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mUtil = nsnull;
  mBehavior = kOnError_Recover;
}

nsEUCJPToUnicode::~nsEUCJPToUnicode() 
{
  NS_IF_RELEASE(mUtil);
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsEUCJPToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsEUCJPToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

// XXX quick hack so I don't have to include nsICharsetConverterManager
extern "C" const nsID kCharsetConverterManagerCID;

NS_IMETHODIMP nsEUCJPToUnicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
                                       PRInt32 * aDestLength, 
                                       const char * aSrc, PRInt32 aSrcOffset, 
                                       PRInt32 * aSrcLength)
{
  if (aDest == NULL) return NS_ERROR_NULL_POINTER;
  if(nsnull == mUtil)
  {
     nsresult res = NS_OK;
     res = nsComponentManager::CreateInstance(
             kCharsetConverterManagerCID, 
             NULL,
             kIUnicodeDecodeUtilIID,
             (void**) & mUtil);
    
     if(NS_FAILED(res))
     {
       *aSrcLength = 0;
       *aDestLength = 0;
       return res;
     }
  }
  return mUtil->Convert( aDest, aDestOffset, aDestLength, 
                                 aSrc, aSrcOffset, aSrcLength,
                                 mBehavior,
				 4,
				 (uRange*) gRanges,
                                 (uShiftTable*) gShiftTables, 			
                                 (uMappingTable*)gMappingTables);
}

NS_IMETHODIMP nsEUCJPToUnicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  mBehavior = aBehavior;
  return NS_OK;
}
