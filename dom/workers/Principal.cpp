/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Principal.h"

#include "jsapi.h"

BEGIN_WORKERS_NAMESPACE

namespace {

JSPrincipals gPrincipal = {
  1
#ifdef DEBUG
  , kJSPrincipalsDebugToken
#endif
};

} // anonymous namespace

JSPrincipals*
GetWorkerPrincipal()
{
  return &gPrincipal;
}

END_WORKERS_NAMESPACE
