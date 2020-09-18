/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheCommon_h
#define mozilla_dom_cache_CacheCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// Cache equivalents of QM_TRY and QM_DEBUG_TRY.
#define CACHE_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::cache, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define CACHE_TRY(...) CACHE_TRY_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define CACHE_DEBUG_TRY(...) CACHE_TRY(__VA_ARGS__)
#else
#  define CACHE_DEBUG_TRY(...)
#endif

// Cache equivalents of QM_TRY_VAR and QM_DEBUG_TRY_VAR.
#define CACHE_TRY_VAR_GLUE(...) \
  QM_TRY_VAR_META(mozilla::dom::cache, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define CACHE_TRY_VAR(...) CACHE_TRY_VAR_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define CACHE_DEBUG_TRY_VAR(...) CACHE_TRY_VAR(__VA_ARGS__)
#else
#  define CACHE_DEBUG_TRY_VAR(...)
#endif

// Cache equivalents of QM_TRY_RETURN and QM_DEBUG_TRY_RETURN.
#define CACHE_TRY_RETURN_GLUE(...)                                   \
  QM_TRY_RETURN_META(mozilla::dom::cache, MOZ_UNIQUE_VAR(tryResult), \
                     ##__VA_ARGS__)
#define CACHE_TRY_RETURN(...) CACHE_TRY_RETURN_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define CACHE_DEBUG_TRY_RETURN(...) CACHE_TRY_RETURN(__VA_ARGS__)
#else
#  define CACHE_DEBUG_TRY_RETURN(...)
#endif

// Cache equivalents of QM_FAIL and QM_DEBUG_FAIL.
#define CACHE_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::cache, ##__VA_ARGS__)
#define CACHE_FAIL(...) CACHE_FAIL_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define CACHE_DEBUG_FAIL(...) CACHE_FAIL(__VA_ARGS__)
#else
#  define CACHE_DEBUG_FAIL(...)
#endif

namespace mozilla::dom::cache {

QM_META_HANDLE_ERROR("Cache"_ns)

}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_CacheCommon_h
