/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#ifndef nsNullPrincipal_h__
#define nsNullPrincipal_h__

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsCOMPtr.h"
#include "nsIContentSecurityPolicy.h"

class nsIURI;

#define NS_NULLPRINCIPAL_CID \
{ 0xa0bd8b42, 0xf6bf, 0x4fb9, \
  { 0x93, 0x42, 0x90, 0xbf, 0xc9, 0xb7, 0xa1, 0xab } }
#define NS_NULLPRINCIPAL_CONTRACTID "@mozilla.org/nullprincipal;1"

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

class nsNullPrincipal MOZ_FINAL : public nsJSPrincipals
{
public:
  nsNullPrincipal();
  
  // Our refcount is managed by nsJSPrincipals.  Use this macro to avoid an
  // extra refcount member.

  // FIXME: bug 327245 -- I sorta wish there were a clean way to share the
  // nsJSPrincipals munging code between the various principal classes without
  // giving up the NS_DECL_NSIPRINCIPAL goodness.
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRINCIPAL
  NS_DECL_NSISERIALIZABLE

  static already_AddRefed<nsNullPrincipal> CreateWithInheritedAttributes(nsIPrincipal *aInheritFrom);

  nsresult Init(uint32_t aAppId = nsIScriptSecurityManager::NO_APP_ID,
                bool aInMozBrowser = false);

  virtual void GetScriptLocation(nsACString &aStr) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void dumpImpl() MOZ_OVERRIDE;
#endif 

 protected:
  virtual ~nsNullPrincipal();

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  uint32_t mAppId;
  bool mInMozBrowser;
};

#endif // nsNullPrincipal_h__
