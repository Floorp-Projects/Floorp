/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDEFAULTURIFIXUP_H
#define NSDEFAULTURIFIXUP_H

#include "nsIURIFixup.h"

class nsDefaultURIFixupInfo;

/* Header file */
class nsDefaultURIFixup : public nsIURIFixup {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURIFIXUP

  nsDefaultURIFixup();

 protected:
  virtual ~nsDefaultURIFixup();

 private:
  /* additional members */
  nsresult FileURIFixup(const nsACString& aStringURI, nsIURI** aURI);
  nsresult ConvertFileToStringURI(const nsACString& aIn, nsCString& aResult);
  nsresult FixupURIProtocol(const nsACString& aIn,
                            nsDefaultURIFixupInfo* aFixupInfo, nsIURI** aURI);
  nsresult KeywordURIFixup(const nsACString& aStringURI,
                           nsDefaultURIFixupInfo* aFixupInfo,
                           bool aIsPrivateContext, nsIInputStream** aPostData);
  nsresult TryKeywordFixupForURIInfo(const nsACString& aStringURI,
                                     nsDefaultURIFixupInfo* aFixupInfo,
                                     bool aIsPrivateContext,
                                     nsIInputStream** aPostData);
  bool PossiblyHostPortUrl(const nsACString& aUrl);
  bool MakeAlternateURI(nsCOMPtr<nsIURI>& aURI);
  bool IsDomainWhitelisted(const nsACString& aAsciiHost,
                           const uint32_t aDotLoc);
};

class nsDefaultURIFixupInfo : public nsIURIFixupInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURIFIXUPINFO

  explicit nsDefaultURIFixupInfo(const nsACString& aOriginalInput);

  friend class nsDefaultURIFixup;

 protected:
  virtual ~nsDefaultURIFixupInfo();

 private:
  nsCOMPtr<nsISupports> mConsumer;
  nsCOMPtr<nsIURI> mPreferredURI;
  nsCOMPtr<nsIURI> mFixedURI;
  bool mFixupChangedProtocol;
  bool mFixupCreatedAlternateURI;
  nsString mKeywordProviderName;
  nsString mKeywordAsSent;
  nsCString mOriginalInput;
};
#endif
