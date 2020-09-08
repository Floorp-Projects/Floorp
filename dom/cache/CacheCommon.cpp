/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheCommon.h"

namespace mozilla::dom::cache {

void HandleError(const char* const aExpr, const char* const aSourceFile,
                 const int32_t aSourceLine) {
  mozilla::dom::quota::LogError(nsLiteralCString("Cache"),
                                nsDependentCString(aExpr),
                                nsDependentCString(aSourceFile), aSourceLine);
}

}  // namespace mozilla::dom::cache
