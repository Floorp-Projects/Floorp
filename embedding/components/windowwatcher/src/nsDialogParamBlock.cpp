/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
 
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

