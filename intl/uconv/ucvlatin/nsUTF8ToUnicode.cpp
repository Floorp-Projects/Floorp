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
#include "nsUTF8ToUnicode.h"
#include "nsUCvLatinDll.h"

//----------------------------------------------------------------------
// Class nsUTF8ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsUTF8ToUnicode, kIUnicodeDecoderIID);

nsUTF8ToUnicode::nsUTF8ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mUtil = nsnull;
  mBehavior = kOnError_Recover;
}

nsUTF8ToUnicode::~nsUTF8ToUnicode() 
{
  NS_IF_RELEASE(mUtil);
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsUTF8ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUTF8ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

static PRInt16 gShiftTable[] =  {
     3, uMultibytesCharset, 
     ShiftCell(u1ByteChar,       1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F), 
     ShiftCell(u2BytesUTF8,      2, 0xC0, 0xDF, 0x00, 0x00, 0x07, 0xFF), 
     ShiftCell(u3BytesUTF8,      3, 0xE0, 0xEF, 0x08, 0x00, 0xFF, 0xFF) 
};

static PRUint16 gMappingTable[] = {
0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0000
};

// XXX quick hack so I don't have to include nsICharsetConverterManager
extern "C" const nsID kCharsetConverterManagerCID;

NS_IMETHODIMP nsUTF8ToUnicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
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
                                 (uShiftTable*) &gShiftTable, 
                                 (uMappingTable*)&gMappingTable);
}

NS_IMETHODIMP nsUTF8ToUnicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsUTF8ToUnicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsUTF8ToUnicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsUTF8ToUnicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  mBehavior = aBehavior;
  return NS_OK;
}
