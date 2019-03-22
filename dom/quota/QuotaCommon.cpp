/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaCommon.h"

namespace mozilla {
namespace dom {
namespace quota {

namespace {

LazyLogModule gLogger("QuotaManager");

}  // namespace

#ifdef NIGHTLY_BUILD
NS_NAMED_LITERAL_CSTRING(kInternalError, "internal");
NS_NAMED_LITERAL_CSTRING(kExternalError, "external");
#endif

LogModule* GetQuotaManagerLogger() { return gLogger; }

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
