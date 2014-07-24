/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPlatformCharset_h__
#define nsPlatformCharset_h__

#include "nsIPlatformCharset.h"

class nsPlatformCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsPlatformCharset();

  NS_IMETHOD Init();
  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsACString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const nsAString& localeName, nsACString& oResult);

private:
  nsCString mCharset;
  nsString mLocale; // remember the locale & charset

  nsresult MapToCharset(nsAString& inANSICodePage, nsACString& outCharset);
  nsresult InitGetCharset(nsACString& oString);
  nsresult VerifyCharset(nsCString &aCharset);

  virtual ~nsPlatformCharset();
};

#endif // nsPlatformCharset_h__


