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
#include "nsRepository.h"
#include "nsSJIS2Unicode.h"
#include "nsUCVJADll.h"

//----------------------------------------------------------------------
// Class nsSJIS2Unicode [implementation]

NS_IMPL_ISUPPORTS(nsSJIS2Unicode, kIUnicodeDecoderIID);

nsSJIS2Unicode::nsSJIS2Unicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mUtil = nsnull;
  mBehavior = kOnError_Recover;
}

nsSJIS2Unicode::~nsSJIS2Unicode() 
{
  NS_IF_RELEASE(mUtil);
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsSJIS2Unicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsSJIS2Unicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

static PRInt16 gShiftTable[] =  {
          4, uMultibytesCharset,
        ShiftCell(u1ByteChar,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1ByteChar,   1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
        ShiftCell(u2BytesChar,  2, 0x81, 0x9F, 0x81, 0x40, 0x9F, 0xFC),
        ShiftCell(u2BytesChar,  2, 0xE0, 0xFC, 0xE0, 0x40, 0xFC, 0xFC)
};

static PRUint16 gMappingTable[] = {
#include "sjis.ut"
};

// XXX quick hack so I don't have to include nsICharsetConverterManager
extern "C" const nsID kCharsetConverterManagerCID;

NS_IMETHODIMP nsSJIS2Unicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
                                       PRInt32 * aDestLength, 
                                       const char * aSrc, PRInt32 aSrcOffset, 
                                       PRInt32 * aSrcLength)
{
  if (aDest == NULL) return NS_ERROR_NULL_POINTER;
  if(nsnull == mUtil)
  {
     nsresult res = NS_OK;
     res = nsRepository::CreateInstance(
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
                                 (uShiftTable*) &gShiftTable, 
                                 (uMappingTable*)&gMappingTable);
}

NS_IMETHODIMP nsSJIS2Unicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  mBehavior = aBehavior;
  return NS_OK;
}
