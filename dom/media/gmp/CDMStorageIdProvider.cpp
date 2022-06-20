/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CDMStorageIdProvider.h"
#include "GMPLog.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsICryptoHash.h"

#ifdef SUPPORT_STORAGE_ID
#  include "rlz/lib/machine_id.h"
#endif

namespace mozilla {

/*static*/
nsCString CDMStorageIdProvider::ComputeStorageId(const nsCString& aOriginSalt) {
#ifndef SUPPORT_STORAGE_ID
  return ""_ns;
#else
  GMP_LOG_DEBUG("CDMStorageIdProvider::ComputeStorageId");

  std::string machineId;
  if (!rlz_lib::GetMachineId(&machineId)) {
    GMP_LOG_DEBUG(
        "CDMStorageIdProvider::ComputeStorageId: get machineId failed.");
    return ""_ns;
  }

  std::string originSalt(aOriginSalt.BeginReading(), aOriginSalt.Length());
  std::string input =
      machineId + originSalt + CDMStorageIdProvider::kBrowserIdentifier;
  nsCOMPtr<nsICryptoHash> hasher;
  nsresult rv = NS_NewCryptoHash(nsICryptoHash::SHA256, getter_AddRefs(hasher));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG(
        "CDMStorageIdProvider::ComputeStorageId: failed to initialize "
        "hash(0x%08" PRIx32 ")",
        static_cast<uint32_t>(rv));
    return ""_ns;
  }

  rv = hasher->Update(reinterpret_cast<const uint8_t*>(input.c_str()),
                      input.size());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG(
        "CDMStorageIdProvider::ComputeStorageId: failed to update "
        "hash(0x%08" PRIx32 ")",
        static_cast<uint32_t>(rv));
    return ""_ns;
  }

  nsCString storageId;
  rv = hasher->Finish(false, storageId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG(
        "CDMStorageIdProvider::ComputeStorageId: failed to get the final hash "
        "result(0x%08" PRIx32 ")",
        static_cast<uint32_t>(rv));
    return ""_ns;
  }
  return storageId;
#endif
}

}  // namespace mozilla
