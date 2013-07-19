/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringBundle_h__
#define nsStringBundle_h__

#include "mozilla/ReentrantMonitor.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"
#include "nsIPersistentProperties2.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsIStringBundleOverride.h"

class nsStringBundle : public nsIStringBundle
{
public:
    // init version
    nsStringBundle(const char* aURLSpec, nsIStringBundleOverride*);
    nsresult LoadProperties();
    virtual ~nsStringBundle();
  
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISTRINGBUNDLE

    nsCOMPtr<nsIPersistentProperties> mProps;

protected:
    //
    // functional decomposition of the funitions repeatively called 
    //
    nsresult GetStringFromID(int32_t aID, nsAString& aResult);
    nsresult GetStringFromName(const nsAString& aName, nsAString& aResult);

    nsresult GetCombinedEnumeration(nsIStringBundleOverride* aOverrideString,
                                    nsISimpleEnumerator** aResult);
private:
    nsCString              mPropertiesURL;
    nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
    mozilla::ReentrantMonitor    mReentrantMonitor;
    bool                         mAttemptedLoad;
    bool                         mLoaded;
    
public:
    static nsresult FormatString(const PRUnichar *formatStr,
                                 const PRUnichar **aParams, uint32_t aLength,
                                 PRUnichar **aResult);
};

/**
 * An extesible implementation of the StringBudle interface.
 *
 * @created         28/Dec/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsExtensibleStringBundle : public nsIStringBundle
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLE

  nsresult Init(const char * aCategory, nsIStringBundleService *);
private:
  
  nsCOMArray<nsIStringBundle> mBundles;
  bool               mLoaded;

public:

  nsExtensibleStringBundle();
  virtual ~nsExtensibleStringBundle();
};



#endif
