/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenURIInFrameParams.h"

NS_IMPL_ISUPPORTS(nsOpenURIInFrameParams, nsIOpenURIInFrameParams)

nsOpenURIInFrameParams::nsOpenURIInFrameParams() :
  mIsPrivate(false)
{
}

nsOpenURIInFrameParams::~nsOpenURIInFrameParams() {
}

/* attribute DOMString referrer; */
NS_IMETHODIMP
nsOpenURIInFrameParams::GetReferrer(nsAString& aReferrer)
{
  aReferrer = mReferrer;
  return NS_OK;
}
NS_IMETHODIMP
nsOpenURIInFrameParams::SetReferrer(const nsAString& aReferrer)
{
  mReferrer = aReferrer;
  return NS_OK;
}

/* attribute boolean isPrivate; */
NS_IMETHODIMP
nsOpenURIInFrameParams::GetIsPrivate(bool* aIsPrivate)
{
  NS_ENSURE_ARG_POINTER(aIsPrivate);
  *aIsPrivate = mIsPrivate;
  return NS_OK;
}
NS_IMETHODIMP
nsOpenURIInFrameParams::SetIsPrivate(bool aIsPrivate)
{
  mIsPrivate = aIsPrivate;
  return NS_OK;
}
