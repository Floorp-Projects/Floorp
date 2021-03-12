/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SimpledbCommon_h
#define mozilla_dom_simpledb_SimpledbCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// XXX Replace all uses by the QM_* variants and remove these aliases
#define SDB_TRY QM_TRY
#define SDB_TRY_UNWRAP QM_TRY_UNWRAP
#define SDB_TRY_INSPECT QM_TRY_INSPECT
#define SDB_TRY_RETURN QM_TRY_RETURN
#define SDB_FAIL QM_FAIL

namespace mozilla {
namespace dom {

extern const char* kPrefSimpleDBEnabled;

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_simpledb_SimpledbCommon_h
