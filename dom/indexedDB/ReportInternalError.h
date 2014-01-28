/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_reportinternalerror_h__
#define mozilla_dom_indexeddb_reportinternalerror_h__

#include "nsDebug.h"

#include "IndexedDatabase.h"

#define IDB_WARNING(x)                                                         \
  mozilla::dom::indexedDB::ReportInternalError(__FILE__, __LINE__, x);         \
  NS_WARNING(x)

#define IDB_REPORT_INTERNAL_ERR()                                              \
  mozilla::dom::indexedDB::ReportInternalError(__FILE__, __LINE__,             \
                                               "UnknownErr")

// Based on NS_ENSURE_TRUE
#define IDB_ENSURE_TRUE(x, ret)                                                \
  do {                                                                         \
    if (MOZ_UNLIKELY(!(x))) {                                                  \
       IDB_REPORT_INTERNAL_ERR();                                              \
       NS_WARNING("IDB_ENSURE_TRUE(" #x ") failed");                           \
       return ret;                                                             \
    }                                                                          \
  } while(0)

// Based on NS_ENSURE_SUCCESS
#define IDB_ENSURE_SUCCESS(res, ret)                                           \
  do {                                                                         \
    nsresult __rv = res; /* Don't evaluate |res| more than once */             \
    if (NS_FAILED(__rv)) {                                                     \
      IDB_REPORT_INTERNAL_ERR();                                               \
      NS_ENSURE_SUCCESS_BODY(res, ret)                                         \
      return ret;                                                              \
    }                                                                          \
  } while(0)


BEGIN_INDEXEDDB_NAMESPACE

void
ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr);

END_INDEXEDDB_NAMESPACE

#endif  // mozilla_dom_indexeddb_reportinternalerror_h__
