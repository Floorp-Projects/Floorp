/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelPolicy.h"

nsChannelPolicy::nsChannelPolicy()
  : mLoadType(0)
{
}

nsChannelPolicy::~nsChannelPolicy()
{
}

NS_IMPL_ISUPPORTS1(nsChannelPolicy, nsIChannelPolicy)

NS_IMETHODIMP
nsChannelPolicy::GetLoadType(uint32_t *aLoadType)
{
    *aLoadType = mLoadType;
    return NS_OK;
}

NS_IMETHODIMP
nsChannelPolicy::SetLoadType(uint32_t aLoadType)
{
    mLoadType = aLoadType;
    return NS_OK;
}

NS_IMETHODIMP
nsChannelPolicy::GetContentSecurityPolicy(nsISupports **aCSP)
{
    *aCSP = mCSP;
    NS_IF_ADDREF(*aCSP);
    return NS_OK;
}

NS_IMETHODIMP
nsChannelPolicy::SetContentSecurityPolicy(nsISupports *aCSP)
{
    mCSP = aCSP;
    return NS_OK;
}
