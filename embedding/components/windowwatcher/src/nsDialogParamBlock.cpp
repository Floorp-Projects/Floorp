/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
#include "nsDialogParamBlock.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsDialogParamBlock, nsIDialogParamBlock)

nsDialogParamBlock::nsDialogParamBlock() : mNumStrings(0), mString(NULL)
{
  NS_INIT_REFCNT();

  for(PRInt32 i = 0; i < kNumInts; i++)
    mInt[i] = 0;
}

nsDialogParamBlock::~nsDialogParamBlock()
{
  delete [] mString;
}

NS_IMETHODIMP nsDialogParamBlock::SetNumberStrings(PRInt32 inNumStrings)
{
  if (mString != NULL)
    return NS_ERROR_ALREADY_INITIALIZED;

  mString = new nsString[inNumStrings];
  if (!mString)
    return NS_ERROR_OUT_OF_MEMORY;
  mNumStrings = inNumStrings;
  return NS_OK;
}


NS_IMETHODIMP nsDialogParamBlock::GetInt(PRInt32 inIndex, PRInt32 *_retval)
{
  nsresult rv = InBounds(inIndex, kNumInts);
  if (rv == NS_OK)
    *_retval = mInt[inIndex];
  return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetInt(PRInt32 inIndex, PRInt32 inInt)
{
  nsresult rv = InBounds(inIndex, kNumInts);
  if (rv == NS_OK)
    mInt[inIndex]= inInt;
  return rv;
}

  
NS_IMETHODIMP nsDialogParamBlock::GetString(PRInt32 inIndex, PRUnichar **_retval)
{
  if (mNumStrings == 0)
    SetNumberStrings(kNumStrings);
  nsresult rv = InBounds(inIndex, mNumStrings);
  if (rv == NS_OK)
    *_retval = mString[inIndex].ToNewUnicode();
  return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetString(PRInt32 inIndex, const PRUnichar *inString)
{
  if (mNumStrings == 0)
    SetNumberStrings(kNumStrings);
  nsresult rv = InBounds(inIndex, mNumStrings);
  if (rv == NS_OK)
    mString[inIndex]= inString;
  return rv;
}

