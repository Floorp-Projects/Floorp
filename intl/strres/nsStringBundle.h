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

class nsStringBundleBase : public nsIStringBundle
{
public:
    nsStringBundleBase(const char* aURLSpec, nsIStringBundleOverride*);

    nsresult ParseProperties(nsIPersistentProperties**);

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISTRINGBUNDLE

    virtual nsresult LoadProperties() = 0;

    const nsCString& BundleURL() const { return mPropertiesURL; }

protected:
    virtual ~nsStringBundleBase();

    virtual nsresult GetStringImpl(const nsACString& aName, nsAString& aResult) = 0;

    virtual nsresult GetSimpleEnumerationImpl(nsISimpleEnumerator** elements) = 0;

    nsCString              mPropertiesURL;
    nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
    mozilla::ReentrantMonitor    mReentrantMonitor;
    bool                         mAttemptedLoad;
    bool                         mLoaded;

    nsresult GetCombinedEnumeration(nsIStringBundleOverride* aOverrideString,
                                    nsISimpleEnumerator** aResult);

    size_t SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf) const override;

public:
    static nsresult FormatString(const char16_t *formatStr,
                                 const char16_t **aParams, uint32_t aLength,
                                 nsAString& aResult);
};

class nsStringBundle : public nsStringBundleBase
{
public:
    nsStringBundle(const char* aURLSpec, nsIStringBundleOverride*);

    NS_DECL_ISUPPORTS_INHERITED

    nsCOMPtr<nsIPersistentProperties> mProps;

    nsresult LoadProperties() override;

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
    virtual ~nsStringBundle();

    nsresult GetStringImpl(const nsACString& aName, nsAString& aResult) override;

    nsresult GetSimpleEnumerationImpl(nsISimpleEnumerator** elements) override;
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
