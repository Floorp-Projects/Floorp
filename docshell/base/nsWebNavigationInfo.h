/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebNavigationInfo_h__
#define nsWebNavigationInfo_h__

#include "nsIWebNavigationInfo.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "mozilla/Attributes.h"

class nsCString;

// Class ID for webnavigationinfo
#define NS_WEBNAVIGATION_INFO_CID \
 { 0xf30bc0a2, 0x958b, 0x4287,{0xbf, 0x62, 0xce, 0x38, 0xba, 0x0c, 0x81, 0x1e}}

class nsWebNavigationInfo MOZ_FINAL : public nsIWebNavigationInfo
{
public:
  nsWebNavigationInfo() {}
  
  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBNAVIGATIONINFO

  nsresult Init();

private:
  ~nsWebNavigationInfo() {}
  
  // Check whether aType is supported.  If this method throws, the
  // value of aIsSupported is not changed.
  nsresult IsTypeSupportedInternal(const nsCString& aType,
                                   uint32_t* aIsSupported);
  
  nsCOMPtr<nsICategoryManager> mCategoryManager;
};

#endif  // nsWebNavigationInfo_h__
