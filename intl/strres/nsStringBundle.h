/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringBundle_h__
#define nsStringBundle_h__

#include "mozilla/ReentrantMonitor.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCOMArray.h"

class nsIPersistentProperties;
class nsIStringBundleOverride;

class nsStringBundle : public nsIStringBundle
{
public:
    // init version
    nsStringBundle(const char* aURLSpec, nsIStringBundleOverride*);
    nsresult LoadProperties();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISTRINGBUNDLE

    nsCOMPtr<nsIPersistentProperties> mProps;

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
    size_t SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
    virtual ~nsStringBundle();

    nsresult GetCombinedEnumeration(nsIStringBundleOverride* aOverrideString,
                                    nsISimpleEnumerator** aResult);
private:
    nsCString              mPropertiesURL;
    nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
    mozilla::ReentrantMonitor    mReentrantMonitor;
    bool                         mAttemptedLoad;
    bool                         mLoaded;

public:
    static nsresult FormatString(const char16_t *formatStr,
                                 const char16_t **aParams, uint32_t aLength,
                                 nsAString& aResult);
};

class nsExtensibleStringBundle;

/**
 * An extensible implementation of the StringBundle interface.
 *
 * @created         28/Dec/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsExtensibleStringBundle final : public nsIStringBundle
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLE

  nsresult Init(const char * aCategory, nsIStringBundleService *);

public:
  nsExtensibleStringBundle();
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  virtual ~nsExtensibleStringBundle();

  nsCOMArray<nsIStringBundle> mBundles;
  bool mLoaded;
};



#endif
