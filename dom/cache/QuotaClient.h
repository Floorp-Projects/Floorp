/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_QuotaClient_h
#define mozilla_dom_cache_QuotaClient_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/quota/Client.h"
#include "mozIStorageConnection.h"

namespace mozilla {
namespace dom {
namespace cache {

already_AddRefed<quota::Client> CreateQuotaClient();

/**
 * The following functions are used to access the directory padding file. The
 * directory padding file lives in DOM Cache base directory
 * (e.g. foo.com/cache/.padding). It is used to keep the current overall padding
 * size for an origin, so that the QuotaManager doesn't need to access the
 * database when getting quota clients' usage.
 *
 * For the directory padding file, it's only accessed on Quota IO thread
 * (for getting current usage) and Cache IO threads (for tracking padding size
 * change). Besides, the padding file is protected by a mutex lock held by
 * CacheQuotaClient.
 *
 * Each padding file should only take 8 bytes (int64_t) to record the overall
 * padding size. Besides, we use the temporary padding file to indicate if the
 * previous action is completed successfully. If the temporary file exists, it
 * represents that the previous action is failed and the content of padding file
 * cannot be trusted, and we need to restore the padding file from the database.
 */

nsresult RestorePaddingFile(nsIFile* aBaseDir, mozIStorageConnection* aConn);

nsresult WipePaddingFile(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir);

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_QuotaClient_h
