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
#include "nsCOMPtr.h"

class nsIURI;

#define NS_NULLPRINCIPAL_CLASSNAME "nullprincipal"
#define NS_NULLPRINCIPAL_CID \
{ 0xdd156d62, 0xd26f, 0x4441, \
 { 0x9c, 0xdb, 0xe8, 0xf0, 0x91, 0x07, 0xc2, 0x73 } }
#define NS_NULLPRINCIPAL_CONTRACTID "@mozilla.org/nullprincipal;1"

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

class nsNullPrincipal : public nsJSPrincipals
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

  nsresult Init();

  virtual void GetScriptLocation(nsACString &aStr) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void dumpImpl() MOZ_OVERRIDE;
#endif 

 protected:
  virtual ~nsNullPrincipal();

  nsCOMPtr<nsIURI> mURI;
};

#endif // nsNullPrincipal_h__
