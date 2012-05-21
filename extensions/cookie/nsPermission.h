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
               const nsACString &aType, 
               PRUint32 aCapability,
               PRUint32 aExpireType,
               PRInt64 aExpireTime);

  virtual ~nsPermission();
  
protected:
  nsCString mHost;
  nsCString mType;
  PRUint32  mCapability;
  PRUint32  mExpireType;
  PRInt64   mExpireTime;
};

#endif // nsPermission_h__
