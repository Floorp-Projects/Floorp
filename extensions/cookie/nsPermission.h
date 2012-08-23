/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPermission_h__
#define nsPermission_h__

#include "nsIPermission.h"
#include "nsString.h"

////////////////////////////////////////////////////////////////////////////////

class nsPermission : public nsIPermission
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSION

  nsPermission(const nsACString &aHost,
               uint32_t aAppId,
               bool aIsInBrowserElement,
               const nsACString &aType,
               uint32_t aCapability,
               uint32_t aExpireType,
               int64_t aExpireTime);

  virtual ~nsPermission() {};

protected:
  nsCString mHost;
  nsCString mType;
  uint32_t  mCapability;
  uint32_t  mExpireType;
  int64_t   mExpireTime;
  uint32_t  mAppId;
  bool      mIsInBrowserElement;
};

#endif // nsPermission_h__
