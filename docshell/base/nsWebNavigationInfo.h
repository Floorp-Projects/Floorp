/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebNavigationInfo_h__
#define nsWebNavigationInfo_h__

#include "nsIWebNavigationInfo.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsStringFwd.h"
#include "mozilla/Attributes.h"

class nsWebNavigationInfo final : public nsIWebNavigationInfo {
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
