/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SimpledbCommon_h
#define mozilla_dom_simpledb_SimpledbCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// SimpleDB equivalents of QM_TRY and QM_DEBUG_TRY.
#define SDB_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define SDB_TRY(...) SDB_TRY_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define SDB_DEBUG_TRY(...) SDB_TRY(__VA_ARGS__)
#else
#  define SDB_DEBUG_TRY(...)
#endif

// SimpleDB equivalents of QM_TRY_VAR and QM_DEBUG_TRY_VAR.
#define SDB_TRY_VAR_GLUE(...)                                        \
  QM_TRY_VAR_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), \
                  ##__VA_ARGS__)
#define SDB_TRY_VAR(...) SDB_TRY_VAR_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define SDB_DEBUG_TRY_VAR(...) SDB_TRY_VAR(__VA_ARGS__)
#else
#  define SDB_DEBUG_TRY_VAR(...)
#endif

// SimpleDB equivalents of QM_TRY_RETURN and QM_DEBUG_TRY_RETURN.
#define SDB_TRY_RETURN_GLUE(...)                                        \
  QM_TRY_RETURN_META(mozilla::dom::simpledb, MOZ_UNIQUE_VAR(tryResult), \
                     ##__VA_ARGS__)
#define SDB_TRY_RETURN(...) SDB_TRY_RETURN_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define SDB_DEBUG_TRY_RETURN(...) SDB_TRY_RETURN(__VA_ARGS__)
#else
#  define SDB_DEBUG_TRY_RETURN(...)
#endif

// SimpleDB equivalents of QM_FAIL and QM_DEBUG_FAIL.
#define SDB_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::simpledb, ##__VA_ARGS__)
#define SDB_FAIL(...) SDB_FAIL_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define SDB_DEBUG_FAIL(...) SDB_FAIL(__VA_ARGS__)
#else
#  define SDB_DEBUG_FAIL(...)
#endif

namespace mozilla {
namespace dom {

extern const char* kPrefSimpleDBEnabled;

namespace simpledb {

QM_META_HANDLE_ERROR("SimpleDB"_ns)

}  // namespace simpledb

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_SimpledbCommon_h
