/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsPrintOptionsImpl.h"


NS_IMPL_ISUPPORTS1(nsPrintOptions, nsIPrintOptions)

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::nsPrintOptions()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::~nsPrintOptions()
{
  /* destructor code */
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::SetMargins(PRInt32 aTop, PRInt32 aLeft, PRInt32 aRight, PRInt32 aBottom)
{
  mTopMargin = aTop;
  mLeftMargin = aLeft;
  mRightMargin = aRight;
  mBottomMargin = aBottom;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::GetMargins(PRInt32 *aTop, PRInt32 *aLeft, PRInt32 *aRight, PRInt32 *aBottom)
{
  *aTop = mTopMargin;
  *aLeft = mLeftMargin;
  *aRight = mRightMargin;
  *aBottom = mBottomMargin;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::ShowNativeDialog()
{

  return NS_OK;
}
