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
   nsUnicodeNormalizer();
   virtual ~nsUnicodeNormalizer();

   NS_DECL_ISUPPORTS 

   NS_IMETHOD NormalizeUnicodeNFD( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFC( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFKD( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFKC( const nsAString& aSrc, nsAString& aDest);

   // low-level access to the composition data needed for HarfBuzz callbacks
   static bool Compose(PRUint32 a, PRUint32 b, PRUint32 *ab);
   static bool DecomposeNonRecursively(PRUint32 comp, PRUint32 *c1, PRUint32 *c2);
};

#endif //nsUnicodeNormalizer_h__

