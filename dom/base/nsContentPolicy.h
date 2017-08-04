/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim: ft=cpp ts=8 sw=4 et tw=78
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsContentPolicy_h__
#define __nsContentPolicy_h__

#include "nsIContentPolicy.h"
#include "nsCategoryCache.h"

/*
 * Implementation of the "@mozilla.org/layout/content-policy;1" contract.
 */

class nsContentPolicy : public nsIContentPolicy
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPOLICY

    nsContentPolicy();

 protected:
    virtual ~nsContentPolicy();

 private:
    //Array of policies
    nsCategoryCache<nsIContentPolicy> mPolicies;

    nsCOMPtr<nsIContentPolicy> mMixedContentBlocker;
    nsCOMPtr<nsIContentPolicy> mCSPService;

    //Helper type for CheckPolicy
    typedef decltype(&nsIContentPolicy::ShouldProcess) CPMethod;

    //Helper method that applies policyMethod across all policies in mPolicies
    // with the given parameters
    nsresult CheckPolicy(CPMethod policyMethod,
                         nsContentPolicyType contentType,
                         nsIURI *aURI, nsIURI *origURI,
                         nsISupports *requestingContext,
                         const nsACString &mimeGuess, nsISupports *extra,
                         nsIPrincipal *requestPrincipal,
                         int16_t *decision);
};

nsresult
NS_NewContentPolicy(nsIContentPolicy **aResult);

#endif /* __nsContentPolicy_h__ */
