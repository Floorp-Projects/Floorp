/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp ts=8 sw=2 et tw=80
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

class nsContentPolicy : public nsIContentPolicy {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY

  nsContentPolicy();

 protected:
  virtual ~nsContentPolicy();

 private:
  // Array of policies
  nsCategoryCache<nsIContentPolicy> mPolicies;

  // Helper type for CheckPolicy
  using CPMethod = decltype(&nsIContentPolicy::ShouldProcess);

  // Helper method that applies policyMethod across all policies in mPolicies
  // with the given parameters
  nsresult CheckPolicy(CPMethod policyMethod, nsIURI* aURI,
                       nsILoadInfo* aLoadInfo, int16_t* decision);
};

nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

#endif /* __nsContentPolicy_h__ */
