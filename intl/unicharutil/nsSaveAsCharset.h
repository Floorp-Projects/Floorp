/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSaveAsCharset_h_
#define nsSaveAsCharset_h_

#include "nsStringFwd.h"
#include "nsISaveAsCharset.h"
#include "nsAutoPtr.h"
#include "mozilla/Encoding.h"
#include "nsString.h"

class nsSaveAsCharset : public nsISaveAsCharset
{
public:

  nsSaveAsCharset();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(const nsACString& aCharset, uint32_t aIgnored, uint32_t aAlsoIgnored) override;

  NS_IMETHOD Convert(const nsAString& ain, nsACString& aOut) override;

  NS_IMETHOD GetCharset(nsACString& aCharset) override;

private:

  virtual ~nsSaveAsCharset();

  const mozilla::Encoding* mEncoding;
};

#endif
