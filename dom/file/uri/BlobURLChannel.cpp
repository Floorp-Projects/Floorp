/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobURLChannel.h"

using namespace mozilla::dom;

BlobURLChannel::BlobURLChannel(nsIURI* aURI,
                               nsILoadInfo* aLoadInfo)
  : mInitialized(false)
{
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

void
BlobURLChannel::InitFailed()
{
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(!mInputStream);
  mInitialized = true;
}

void
BlobURLChannel::Initialize(BlobImpl* aBlobImpl)
{
  MOZ_ASSERT(!mInitialized);

  nsAutoString contentType;
  aBlobImpl->GetType(contentType);
  SetContentType(NS_ConvertUTF16toUTF8(contentType));

  if (aBlobImpl->IsFile()) {
    nsString filename;
    aBlobImpl->GetName(filename);
    SetContentDispositionFilename(filename);
  }

  ErrorResult rv;
  uint64_t size = aBlobImpl->GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    InitFailed();
    return;
  }

  SetContentLength(size);

  aBlobImpl->CreateInputStream(getter_AddRefs(mInputStream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    InitFailed();
    return;
  }

  MOZ_ASSERT(mInputStream);
  mInitialized = true;
}

nsresult
BlobURLChannel::OpenContentStream(bool aAsync, nsIInputStream** aResult,
                                  nsIChannel** aChannel)
{
  MOZ_ASSERT(mInitialized);

  if (!mInputStream) {
    return NS_ERROR_MALFORMED_URI;
  }

  EnableSynthesizedProgressEvents(true);

  nsCOMPtr<nsIInputStream> stream = mInputStream;
  stream.forget(aResult);

  return NS_OK;
}

void
BlobURLChannel::OnChannelDone()
{
  mInputStream = nullptr;
}
