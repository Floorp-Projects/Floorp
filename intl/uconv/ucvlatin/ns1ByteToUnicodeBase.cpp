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
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsICharsetConverterManager.h"
#include "ns1ByteToUnicodeBase.h"

ns1ByteToUnicodeBase::ns1ByteToUnicodeBase() 
{
  mUtil = nsnull;
}

ns1ByteToUnicodeBase::~ns1ByteToUnicodeBase() 
{
  NS_IF_RELEASE(mUtil);
}



NS_IMETHODIMP ns1ByteToUnicodeBase::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
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

     if( PR_FALSE == GetFastTableInitState()) 
     {
         res = mUtil->Init1ByteFastTable(
                    this->GetMappingTable(),
                    this->GetFastTable());
         if(NS_FAILED(res))
         {
           *aSrcLength = 0;
           *aDestLength = 0;
           return res;
         }
         this->SetFastTableInit();
     }
  }
  return mUtil->Convert( aDest, aDestOffset, aDestLength, 
                                 aSrc, aSrcOffset, aSrcLength,
                                 this->GetFastTable());
}

NS_IMETHODIMP ns1ByteToUnicodeBase::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP ns1ByteToUnicodeBase::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_EXACT_LENGTH;
}

NS_IMETHODIMP ns1ByteToUnicodeBase::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP ns1ByteToUnicodeBase::SetInputErrorBehavior(PRInt32 aBehavior)
{
  return NS_OK;
}
