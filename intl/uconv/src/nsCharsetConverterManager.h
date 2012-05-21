/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCharsetConverterManager_h__
#define nsCharsetConverterManager_h__

#include "nsISupports.h"
#include "nsICharsetConverterManager.h"
#include "nsIStringBundle.h"
#include "nsInterfaceHashtable.h"
#include "mozilla/Mutex.h"

class nsCharsetConverterManager : public nsICharsetConverterManager
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHARSETCONVERTERMANAGER

public:

  nsCharsetConverterManager();
  virtual ~nsCharsetConverterManager();

private:

  nsIStringBundle * mDataBundle;
  nsIStringBundle * mTitleBundle;

  nsresult LoadExtensibleBundle(const char * aRegistryKey, 
      nsIStringBundle ** aResult);

  nsresult GetBundleValue(nsIStringBundle * aBundle,
                          const char * aName, 
                          const nsAFlatString& aProp, PRUnichar ** aResult);
  nsresult GetBundleValue(nsIStringBundle * aBundle,
                          const char * aName, 
                          const nsAFlatString& aProp, nsAString& aResult);

  nsresult GetList(const nsACString& aCategory,
                   const nsACString& aPrefix,
                   nsIUTF8StringEnumerator** aResult);
};

#endif // nsCharsetConverterManager_h__


