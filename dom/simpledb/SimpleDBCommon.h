/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SimpledbCommon_h
#define mozilla_dom_simpledb_SimpledbCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsLiteralString.h"

// SimpleDB equivalents of QM_TRY.
#define SDB_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define SDB_TRY(...) SDB_TRY_GLUE(__VA_ARGS__)

// SimpleDB equivalents of QM_TRY_UNWRAP and QM_TRY_INSPECT.
#define SDB_TRY_ASSIGN_GLUE(accessFunction, ...)                        \
  QM_TRY_ASSIGN_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), \
                     accessFunction, ##__VA_ARGS__)
#define SDB_TRY_UNWRAP(...) SDB_TRY_ASSIGN_GLUE(unwrap, __VA_ARGS__)
#define SDB_TRY_INSPECT(...) SDB_TRY_ASSIGN_GLUE(inspect, __VA_ARGS__)

// SimpleDB equivalents of QM_TRY_RETURN.
#define SDB_TRY_RETURN_GLUE(...)                                        \
  QM_TRY_RETURN_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), \
                     ##__VA_ARGS__)
#define SDB_TRY_RETURN(...) SDB_TRY_RETURN_GLUE(__VA_ARGS__)

// SimpleDB equivalents of QM_FAIL.
#define SDB_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::simpledb, ##__VA_ARGS__)
#define SDB_FAIL(...) SDB_FAIL_GLUE(__VA_ARGS__)

namespace mozilla {
namespace dom {

extern const char* kPrefSimpleDBEnabled;

namespace simpledb {

QM_META_HANDLE_ERROR("SimpleDB"_ns)

}  // namespace simpledb

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_SimpledbCommon_h
