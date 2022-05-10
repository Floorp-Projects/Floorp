/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_reportinternalerror_h__
#define mozilla_dom_indexeddb_reportinternalerror_h__

#include "nsDebug.h"
#include "nsPrintfCString.h"

#include "IndexedDatabase.h"

#define IDB_WARNING(...)                                                       \
  do {                                                                         \
    nsPrintfCString s(__VA_ARGS__);                                            \
    mozilla::dom::indexedDB::ReportInternalError(__FILE__, __LINE__, s.get()); \
    NS_WARNING(s.get());                                                       \
  } while (0)

#define IDB_REPORT_INTERNAL_ERR() \
  mozilla::dom::indexedDB::ReportInternalError(__FILE__, __LINE__, "UnknownErr")

#define IDB_REPORT_INTERNAL_ERR_LAMBDA \
  [](const auto&) { IDB_REPORT_INTERNAL_ERR(); }

namespace mozilla::dom::indexedDB {

MOZ_COLD void ReportInternalError(const char* aFile, uint32_t aLine,
                                  const char* aStr);

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_reportinternalerror_h__
