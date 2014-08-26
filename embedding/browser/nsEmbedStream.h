/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEmbedStream_h__
#define nsEmbedStream_h__

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIWebBrowser.h"

class nsEmbedStream : public nsISupports
{
 public:

  nsEmbedStream();

  void      InitOwner      (nsIWebBrowser *aOwner);
  NS_METHOD Init           (void);

  NS_METHOD OpenStream     (nsIURI *aBaseURI, const nsACString& aContentType);
  NS_METHOD AppendToStream (const uint8_t *aData, uint32_t aLen);
  NS_METHOD CloseStream    (void);

  NS_DECL_ISUPPORTS

 protected:
  virtual ~nsEmbedStream();

 private:
  nsIWebBrowser            *mOwner;
  nsCOMPtr<nsIOutputStream> mOutputStream;
};

#endif // nsEmbedStream_h__
