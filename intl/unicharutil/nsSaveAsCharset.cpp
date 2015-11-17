/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSaveAsCharset.h"
#include "mozilla/dom/EncodingUtils.h"

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
  nsAutoCString encoding;
  if (!mozilla::dom::EncodingUtils::FindEncodingForLabelNoReplacement(aCharset, encoding)) {
    return NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR;
  }
  mEncoder = new nsNCRFallbackEncoderWrapper(encoding);
  mCharset.Assign(encoding);
  return NS_OK;
}

NS_IMETHODIMP
nsSaveAsCharset::Convert(const nsAString& aIn, nsACString& aOut)
{
  if (!mEncoder) {
    return NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR;
  }

  if (!mEncoder->Encode(aIn, aOut)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsSaveAsCharset::GetCharset(nsACString& aCharset)
{
  aCharset.Assign(mCharset);
  return NS_OK;
}
