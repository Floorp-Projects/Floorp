/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintProgressParams.h"
#include "nsReadableUtils.h"


NS_IMPL_ISUPPORTS1(nsPrintProgressParams, nsIPrintProgressParams)

nsPrintProgressParams::nsPrintProgressParams()
{
}

nsPrintProgressParams::~nsPrintProgressParams()
{
}

/* attribute wstring docTitle; */
NS_IMETHODIMP nsPrintProgressParams::GetDocTitle(PRUnichar * *aDocTitle)
{
  NS_ENSURE_ARG(aDocTitle);
  
  *aDocTitle = ToNewUnicode(mDocTitle);
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgressParams::SetDocTitle(const PRUnichar * aDocTitle)
{
  mDocTitle = aDocTitle;
  return NS_OK;
}

/* attribute wstring docURL; */
NS_IMETHODIMP nsPrintProgressParams::GetDocURL(PRUnichar * *aDocURL)
{
  NS_ENSURE_ARG(aDocURL);
  
  *aDocURL = ToNewUnicode(mDocURL);
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgressParams::SetDocURL(const PRUnichar * aDocURL)
{
  mDocURL = aDocURL;
  return NS_OK;
}

