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
#include "nsLatin1ToUnicode.h"
#include "nsUCvLatinCID.h"
#include "nsUCvLatinDll.h"

//----------------------------------------------------------------------
// Class nsLatin1ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsLatin1ToUnicode, kIUnicodeDecoderIID);

nsLatin1ToUnicode::nsLatin1ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsLatin1ToUnicode::~nsLatin1ToUnicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsLatin1ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsLatin1ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

NS_IMETHODIMP nsLatin1ToUnicode::Convert(PRUnichar * aDest, 
                                         PRInt32 aDestOffset,
                                         PRInt32 * aDestLength, 
                                         const char * aSrc, 
                                         PRInt32 aSrcOffset, 
                                         PRInt32 * aSrcLength)
{
  // XXX hello, this isn't ASCII! So do a table-driven mapping for Latin1

  aSrc += aSrcOffset;
  aDest += aDestOffset;
  PRInt32 len = PR_MIN(*aSrcLength, *aDestLength);
  const char * srcEnd = aSrc+len;

  for (;aSrc<srcEnd;) *aDest++ = ((PRUint8)*aSrc++);

  nsresult res = (*aSrcLength > len)? NS_PARTIAL_MORE_OUTPUT : NS_OK;
  *aSrcLength = *aDestLength = len;

  return res;
}

NS_IMETHODIMP nsLatin1ToUnicode::Finish(PRUnichar * aDest, 
                                        PRInt32 aDestOffset,
                                        PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsLatin1ToUnicode::Length(const char * aSrc, 
                                        PRInt32 aSrcOffset, 
                                        PRInt32 aSrcLength, 
                                        PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_EXACT_LENGTH;
}

NS_IMETHODIMP nsLatin1ToUnicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsLatin1ToUnicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  // no input error possible, this encoding is too simple
  return NS_OK;
}
