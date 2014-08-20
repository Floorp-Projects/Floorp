/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "nsDialogParamBlock.h"
#include "nsString.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS(nsDialogParamBlock, nsIDialogParamBlock)

nsDialogParamBlock::nsDialogParamBlock() : mNumStrings(0), mString(nullptr)
{
  for(int32_t i = 0; i < kNumInts; i++)
    mInt[i] = 0;
}

nsDialogParamBlock::~nsDialogParamBlock()
{
  delete [] mString;
}

NS_IMETHODIMP nsDialogParamBlock::SetNumberStrings(int32_t inNumStrings)
{
  if (mString != nullptr)
    return NS_ERROR_ALREADY_INITIALIZED;

  mString = new nsString[inNumStrings];
  if (!mString)
    return NS_ERROR_OUT_OF_MEMORY;
  mNumStrings = inNumStrings;
  return NS_OK;
}


NS_IMETHODIMP nsDialogParamBlock::GetInt(int32_t inIndex, int32_t *_retval)
{
  nsresult rv = InBounds(inIndex, kNumInts);
  if (rv == NS_OK)
    *_retval = mInt[inIndex];
  return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetInt(int32_t inIndex, int32_t inInt)
{
  nsresult rv = InBounds(inIndex, kNumInts);
  if (rv == NS_OK)
    mInt[inIndex]= inInt;
  return rv;
}

  
NS_IMETHODIMP nsDialogParamBlock::GetString(int32_t inIndex, char16_t **_retval)
{
  if (mNumStrings == 0)
    SetNumberStrings(kNumStrings);
  nsresult rv = InBounds(inIndex, mNumStrings);
  if (rv == NS_OK)
    *_retval = ToNewUnicode(mString[inIndex]);
  return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetString(int32_t inIndex, const char16_t *inString)
{
  if (mNumStrings == 0)
    SetNumberStrings(kNumStrings);
  nsresult rv = InBounds(inIndex, mNumStrings);
  if (rv == NS_OK)
    mString[inIndex]= inString;
  return rv;
}

NS_IMETHODIMP
nsDialogParamBlock::GetObjects(nsIMutableArray * *aObjects)
{
  NS_ENSURE_ARG_POINTER(aObjects);
  NS_IF_ADDREF(*aObjects = mObjects);
  return NS_OK;
}

NS_IMETHODIMP
nsDialogParamBlock::SetObjects(nsIMutableArray * aObjects)
{
  mObjects = aObjects;
  return NS_OK;
}


