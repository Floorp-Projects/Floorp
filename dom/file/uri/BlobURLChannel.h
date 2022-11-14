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

namespace mozilla::dom {

class BlobImpl;

class BlobURLChannel final : public nsBaseChannel {
 public:
  BlobURLChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  NS_IMETHOD SetContentType(const nsACString& aContentType) override;

 private:
  ~BlobURLChannel() override;

  nsresult OpenContentStream(bool aAsync, nsIInputStream** aResult,
                             nsIChannel** aChannel) override;

  bool mContentStreamOpened;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_BlobURLChannel_h */
