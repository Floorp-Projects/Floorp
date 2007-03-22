/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

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

