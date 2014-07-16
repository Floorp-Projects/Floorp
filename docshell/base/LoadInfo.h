/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIContentPolicy.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsIWeakReferenceUtils.h" // for nsWeakPtr

class nsINode;

namespace mozilla {

/**
 * Class that provides an nsILoadInfo implementation.
 */
class MOZ_EXPORT LoadInfo MOZ_FINAL : public nsILoadInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADINFO

  // aPrincipal MUST NOT BE NULL.
  LoadInfo(nsIPrincipal* aPrincipal,
           nsINode* aLoadingContext,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType);

private:
  ~LoadInfo();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsWeakPtr              mLoadingContext;
  nsSecurityFlags        mSecurityFlags;
  nsContentPolicyType    mContentPolicyType;
};

} // namespace mozilla

#endif // mozilla_LoadInfo_h

