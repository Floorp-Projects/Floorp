/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ReportInternalError_h
#define mozilla_dom_localstorage_ReportInternalError_h

#include <cstdint>
#include "mozilla/Attributes.h"
#include "nsDebug.h"

#define LS_WARNING(...)                                                 \
  do {                                                                  \
    nsPrintfCString s(__VA_ARGS__);                                     \
    mozilla::dom::localstorage::ReportInternalError(__FILE__, __LINE__, \
                                                    s.get());           \
    NS_WARNING(s.get());                                                \
  } while (0)

namespace mozilla::dom::localstorage {

MOZ_COLD void ReportInternalError(const char* aFile, uint32_t aLine,
                                  const char* aStr);

}  // namespace mozilla::dom::localstorage

#endif  // mozilla_dom_localstorage_ReportInternalError_h
