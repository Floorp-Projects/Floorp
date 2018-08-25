/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "FuzzingInterface.h"
#include "nsCSPContext.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"

static int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  nsresult ret;
  nsCOMPtr<nsIURI> selfURI;
  ret = NS_NewURI(getter_AddRefs(selfURI), "http://selfuri.com");
  if (ret != NS_OK)
    return 0;

  mozilla::OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> selfURIPrincipal =
    mozilla::BasePrincipal::CreateCodebasePrincipal(selfURI, attrs);
  if (!selfURIPrincipal)
    return 0;

  nsCOMPtr<nsIContentSecurityPolicy> csp =
    do_CreateInstance(NS_CSPCONTEXT_CONTRACTID, &ret);
  if (ret != NS_OK)
    return 0;

  ret = csp->SetRequestContext(nullptr, selfURIPrincipal);
  if (ret != NS_OK)
    return 0;

  NS_ConvertASCIItoUTF16 policy(reinterpret_cast<const char*>(data), size);
  if (!policy.get())
    return 0;
  csp->AppendPolicy(policy, false, false);

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(nullptr, LLVMFuzzerTestOneInput, ContentSecurityPolicyParser);

