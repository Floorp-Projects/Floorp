/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SimpledbCommon_h
#define mozilla_dom_simpledb_SimpledbCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// SimpleDB equivalent of QM_TRY.
#define SDB_TRY(...) QM_TRY_META(mozilla::dom::simpledb, ##__VA_ARGS__)

// SimpleDB equivalent of QM_TRY_VAR.
#define SDB_TRY_VAR(...) QM_TRY_VAR_META(mozilla::dom::simpledb, ##__VA_ARGS__)

namespace mozilla {
namespace dom {

extern const char* kPrefSimpleDBEnabled;

namespace simpledb {

void HandleError(const nsLiteralCString& aExpr,
                 const nsLiteralCString& aSourceFile, int32_t aSourceLine);

}  // namespace simpledb

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_SimpledbCommon_h
