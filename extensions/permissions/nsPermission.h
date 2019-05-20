/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPermission_h__
#define nsPermission_h__

#include "nsIPermission.h"
#include "nsCOMPtr.h"
#include "nsString.h"

////////////////////////////////////////////////////////////////////////////////

class nsPermission : public nsIPermission {
 public:
  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSION

  static already_AddRefed<nsPermission> Create(
      nsIPrincipal* aPrincipal, const nsACString& aType, uint32_t aCapability,
      uint32_t aExpireType, int64_t aExpireTime, int64_t aModificationTime);

  // This method creates a new nsIPrincipal with a stripped OriginAttributes (no
  // userContextId, and no FirstPartyDomain) and a codebase equal to the origin
  // of 'aPrincipal'.
  static already_AddRefed<nsIPrincipal> ClonePrincipalForPermission(
      nsIPrincipal* aPrincipal);

 protected:
  nsPermission(nsIPrincipal* aPrincipal, const nsACString& aType,
               uint32_t aCapability, uint32_t aExpireType, int64_t aExpireTime,
               int64_t aModificationTime);

  virtual ~nsPermission(){};

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
  uint32_t mCapability;
  uint32_t mExpireType;
  int64_t mExpireTime;
  int64_t mModificationTime;
};

#endif  // nsPermission_h__
