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
    : mInitialized(false) {
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

void BlobURLChannel::InitFailed() {
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(!mInputStream);
  mInitialized = true;
}

void BlobURLChannel::Initialize() {
  MOZ_ASSERT(!mInitialized);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    InitFailed();
    return;
  }

  RefPtr<BlobURL> blobURL;
  rv = uri->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(blobURL));

  if (NS_FAILED(rv) || !blobURL) {
    InitFailed();
    return;
  }

  mInputStream = BlobURLInputStream::Create(this, blobURL);
  if (NS_WARN_IF(!mInputStream)) {
    InitFailed();
    return;
  }

  mInitialized = true;
}

nsresult BlobURLChannel::OpenContentStream(bool aAsync,
                                           nsIInputStream** aResult,
                                           nsIChannel** aChannel) {
  MOZ_ASSERT(mInitialized);

  if (!mInputStream) {
    return NS_ERROR_MALFORMED_URI;
  }

  EnableSynthesizedProgressEvents(true);

  nsCOMPtr<nsIInputStream> stream = mInputStream;
  stream.forget(aResult);

  return NS_OK;
}

void BlobURLChannel::OnChannelDone() { mInputStream = nullptr; }
