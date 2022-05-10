/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptObjectPrincipal_h__
#define nsIScriptObjectPrincipal_h__

#include "nsISupports.h"

class nsIPrincipal;

#define NS_ISCRIPTOBJECTPRINCIPAL_IID                \
  {                                                  \
    0x3eedba38, 0x8d22, 0x41e1, {                    \
      0x81, 0x7a, 0x0e, 0x43, 0xe1, 0x65, 0xb6, 0x64 \
    }                                                \
  }

/**
 * JS Object Principal information.
 */
class nsIScriptObjectPrincipal : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTOBJECTPRINCIPAL_IID)

  virtual nsIPrincipal* GetPrincipal() = 0;

  virtual nsIPrincipal* GetEffectiveCookiePrincipal() = 0;

  virtual nsIPrincipal* GetEffectiveStoragePrincipal() = 0;

  virtual nsIPrincipal* PartitionedPrincipal() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptObjectPrincipal,
                              NS_ISCRIPTOBJECTPRINCIPAL_IID)

#endif  // nsIScriptObjectPrincipal_h__
