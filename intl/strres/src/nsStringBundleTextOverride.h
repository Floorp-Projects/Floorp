/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsStringBundleTextOverride_h__
#define nsStringBundleTextOverride_h__

#include "nsIStringBundleOverride.h"
#include "nsCOMPtr.h"
#include "nsIPersistentProperties2.h"

// {6316C6CE-12D3-479e-8F53-E289351412B8}
#define NS_STRINGBUNDLETEXTOVERRIDE_CID \
  { 0x6316c6ce, 0x12d3, 0x479e, \
  { 0x8f, 0x53, 0xe2, 0x89, 0x35, 0x14, 0x12, 0xb8 } }


#define NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID \
    "@mozilla.org/intl/stringbundle/text-override;1"

// an implementation which does overrides from a text file

class nsStringBundleTextOverride : public nsIStringBundleOverride
{
 public:
    nsStringBundleTextOverride() { }
    virtual ~nsStringBundleTextOverride() {}

    nsresult Init();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTRINGBUNDLEOVERRIDE

private:
    nsCOMPtr<nsIPersistentProperties> mValues;
    
};

#endif
