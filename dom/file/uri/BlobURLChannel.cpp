/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobURLChannel.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BlobURL.h"
#include "mozilla/dom/BlobURLInputStream.h"

using namespace mozilla::dom;

BlobURLChannel::BlobURLChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo)
    : mContentStreamOpened(false) {
  SetURI(aURI);
  SetOriginalURI(aURI);
  SetLoadInfo(aLoadInfo);

  // If we're sandboxed, make sure to clear any owner the channel
  // might already have.
  if (aLoadInfo && aLoadInfo->GetLoadingSandboxed()) {
    SetOwner(nullptr);
  }
}

BlobURLChannel::~BlobURLChannel() = default;

NS_IMETHODIMP
BlobURLChannel::SetContentType(const nsACString& aContentType) {
  // If the blob type is empty, set the content type of the channel to the
  // empty string.
  if (aContentType.IsEmpty()) {
    mContentType.Truncate();
    return NS_OK;
  }

  return nsBaseChannel::SetContentType(aContentType);
}

nsresult BlobURLChannel::OpenContentStream(bool aAsync,
                                           nsIInputStream** aResult,
                                           nsIChannel** aChannel) {
  if (mContentStreamOpened) {
    return NS_ERROR_ALREADY_OPENED;
  }

  mContentStreamOpened = true;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_MALFORMED_URI);

  RefPtr<BlobURL> blobURL;
  rv = uri->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(blobURL));

  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!blobURL)) {
    return NS_ERROR_MALFORMED_URI;
  }

  if (blobURL->Revoked()) {
#ifdef MOZ_WIDGET_ANDROID
    nsCOMPtr<nsILoadInfo> loadInfo;
    GetLoadInfo(getter_AddRefs(loadInfo));
    // if the channel was not triggered by the system principal,
    // then we return here because the URL had been revoked
    if (loadInfo && !loadInfo->TriggeringPrincipal()->IsSystemPrincipal()) {
      return NS_ERROR_MALFORMED_URI;
    }
#else
    return NS_ERROR_MALFORMED_URI;
#endif
  }

  nsCOMPtr<nsIInputStream> inputStream =
      BlobURLInputStream::Create(this, blobURL);
  if (NS_WARN_IF(!inputStream)) {
    return NS_ERROR_MALFORMED_URI;
  }

  EnableSynthesizedProgressEvents(true);

  inputStream.forget(aResult);

  return NS_OK;
}
