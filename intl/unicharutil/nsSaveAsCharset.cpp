/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSaveAsCharset.h"

using namespace mozilla;

//
// nsISupports methods
//
NS_IMPL_ISUPPORTS(nsSaveAsCharset, nsISaveAsCharset)

//
// nsSaveAsCharset
//
nsSaveAsCharset::nsSaveAsCharset()
{
}

nsSaveAsCharset::~nsSaveAsCharset()
{
}

NS_IMETHODIMP
nsSaveAsCharset::Init(const nsACString& aCharset, uint32_t aIgnored, uint32_t aAlsoIgnored)
{
  mEncoding = Encoding::ForLabelNoReplacement(aCharset);
  if (!mEncoding) {
    return NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsCharset::Convert(const nsAString& aIn, nsACString& aOut)
{
  if (!mEncoding) {
    return NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR;
  }
  nsresult rv;
  const Encoding* ignored;
  Tie(rv, ignored) = mEncoding->Encode(aIn, aOut);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsCharset::GetCharset(nsACString& aCharset)
{
  if (!mEncoding) {
    aCharset.Truncate();
  } else {
    mEncoding->Name(aCharset);
  }
  return NS_OK;
}
