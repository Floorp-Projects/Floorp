/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginPlayPreviewInfo.h"

using namespace mozilla;

nsPluginPlayPreviewInfo::nsPluginPlayPreviewInfo(const char* aMimeType,
                                                 bool aIgnoreCTP,
                                                 const char* aRedirectURL)
  : mMimeType(aMimeType), mIgnoreCTP(aIgnoreCTP), mRedirectURL(aRedirectURL) {}

nsPluginPlayPreviewInfo::nsPluginPlayPreviewInfo(
  const nsPluginPlayPreviewInfo* aSource)
{
  MOZ_ASSERT(aSource);

  mMimeType = aSource->mMimeType;
  mIgnoreCTP = aSource->mIgnoreCTP;
  mRedirectURL = aSource->mRedirectURL;
}

nsPluginPlayPreviewInfo::~nsPluginPlayPreviewInfo()
{
}

NS_IMPL_ISUPPORTS1(nsPluginPlayPreviewInfo, nsIPluginPlayPreviewInfo)

NS_IMETHODIMP
nsPluginPlayPreviewInfo::GetMimeType(nsACString& aMimeType)
{
  aMimeType = mMimeType;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginPlayPreviewInfo::GetIgnoreCTP(bool* aIgnoreCTP)
{
  *aIgnoreCTP = mIgnoreCTP;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginPlayPreviewInfo::GetRedirectURL(nsACString& aRedirectURL)
{
  aRedirectURL = mRedirectURL;
  return NS_OK;
}
