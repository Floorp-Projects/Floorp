/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobURLChannel_h
#define mozilla_dom_BlobURLChannel_h

#include "nsBaseChannel.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

class nsIURI;

namespace mozilla {
namespace dom {

class BlobImpl;

class BlobURLChannel final : public nsBaseChannel
{
public:
  BlobURLChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  // This method is called when there is not a valid BlobImpl for this channel.
  // This method will make ::OpenContentStream to return NS_ERROR_MALFORMED_URI.
  void InitFailed();

  // There is a valid BlobImpl for the channel. The blob's inputStream will be
  // used when ::OpenContentStream is called.
  void Initialize(BlobImpl* aBlobImpl);

private:
  ~BlobURLChannel();

  nsresult OpenContentStream(bool aAsync, nsIInputStream** aResult,
                             nsIChannel** aChannel) override;

  void OnChannelDone() override;

  // If Initialize() is called, this will contain the blob's inputStream.
  nsCOMPtr<nsIInputStream> mInputStream;

  // This boolean is used to check that InitFailed() or Initialize() are called
  // just once.
  bool mInitialized;
};

} // dom namespace
} // mozilla namespace

#endif /* mozilla_dom_BlobURLChannel_h */
