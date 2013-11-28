/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Principal.h"

#include "jsapi.h"

BEGIN_WORKERS_NAMESPACE

JSPrincipals*
GetWorkerPrincipal()
{
  static Atomic<uint32_t> sInitialized(0);
  static JSPrincipals sPrincipal;

  uint32_t isInitialized = sInitialized.exchange(1);
  if (!isInitialized) {
    sPrincipal.refcount = 1;
#ifdef DEBUG
    sPrincipal.debugToken = kJSPrincipalsDebugToken;
#endif
  }
  return &sPrincipal;
}

END_WORKERS_NAMESPACE
