/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Types_h
#define mozilla_dom_cache_Types_h

#include <functional>
#include <stdint.h>
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace cache {

enum Namespace {
  DEFAULT_NAMESPACE,
  CHROME_ONLY_NAMESPACE,
  NUMBER_OF_NAMESPACES
};
static const Namespace INVALID_NAMESPACE = NUMBER_OF_NAMESPACES;

typedef int64_t CacheId;
static const CacheId INVALID_CACHE_ID = -1;

// XXX Rename to OriginMetadata.
struct QuotaInfo : quota::OriginMetadata {
  nsCOMPtr<nsIFile> mDir;
  int64_t mDirectoryLockId = -1;
};

struct DeletionInfo {
  nsTArray<nsID> mDeletedBodyIdList;
  int64_t mDeletedPaddingSize = 0;
};

typedef std::function<void(nsCOMPtr<nsIInputStream>&&)> InputStreamResolver;

enum class OpenMode : uint8_t { Eager, Lazy, NumTypes };

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_Types_h
