/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasePrincipal_h
#define mozilla_BasePrincipal_h

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"

namespace mozilla {

/*
 * Base class from which all nsIPrincipal implementations inherit. Use this for
 * default implementations and other commonalities between principal
 * implementations.
 *
 * We should merge nsJSPrincipals into this class at some point.
 */
class BasePrincipal : public nsJSPrincipals
{
public:
  BasePrincipal() {}
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp);
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp);
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;

  virtual bool IsOnCSSUnprefixingWhitelist() override { return false; }

protected:
  virtual ~BasePrincipal() {}

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
};

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
