/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeNormalizer_h__
#define nsUnicodeNormalizer_h__

#include "nscore.h"
#include "nsISupports.h"

#include "nsIUnicodeNormalizer.h"

nsresult NS_NewUnicodeNormalizer(nsISupports** oResult);


class nsUnicodeNormalizer : public nsIUnicodeNormalizer {
public:
   nsUnicodeNormalizer() { }

   NS_DECL_ISUPPORTS

   NS_IMETHOD NormalizeUnicodeNFD( const nsAString& aSrc, nsAString& aDest) override;
   NS_IMETHOD NormalizeUnicodeNFC( const nsAString& aSrc, nsAString& aDest) override;
   NS_IMETHOD NormalizeUnicodeNFKD( const nsAString& aSrc, nsAString& aDest) override;
   NS_IMETHOD NormalizeUnicodeNFKC( const nsAString& aSrc, nsAString& aDest) override;

private:
   virtual ~nsUnicodeNormalizer() { }
};

#endif //nsUnicodeNormalizer_h__

