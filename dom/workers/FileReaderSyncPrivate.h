/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMFileReaderSyncPrivate_h
#define nsDOMFileReaderSyncPrivate_h

#include "Workers.h"

#include "nsICharsetDetectionObserver.h"
#include "nsStringGlue.h"
#include "mozilla/Attributes.h"

class nsIInputStream;
class nsIDOMBlob;

BEGIN_WORKERS_NAMESPACE

class FileReaderSyncPrivate MOZ_FINAL : public PrivatizableBase,
                                        public nsICharsetDetectionObserver
{
  nsCString mCharset;
  nsresult ConvertStream(nsIInputStream *aStream, const char *aCharset,
                         nsAString &aResult);
  nsresult GuessCharset(nsIInputStream *aStream, nsACString &aCharset);

public:
  NS_DECL_ISUPPORTS

  FileReaderSyncPrivate();
  ~FileReaderSyncPrivate();

  nsresult ReadAsArrayBuffer(nsIDOMBlob* aBlob, PRUint32 aLength,
                             uint8* aBuffer);
  nsresult ReadAsBinaryString(nsIDOMBlob* aBlob, nsAString& aResult);
  nsresult ReadAsText(nsIDOMBlob* aBlob, const nsAString& aEncoding,
                      nsAString& aResult);
  nsresult ReadAsDataURL(nsIDOMBlob* aBlob, nsAString& aResult);

  // From nsICharsetDetectionObserver
  NS_IMETHOD Notify(const char *aCharset, nsDetectionConfident aConf);
};

END_WORKERS_NAMESPACE

#endif
