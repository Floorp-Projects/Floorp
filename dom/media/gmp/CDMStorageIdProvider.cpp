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
#include "rlz/lib/machine_id.h"
#endif

namespace mozilla {

/*static*/ nsCString
CDMStorageIdProvider::ComputeStorageId(const nsCString& aOriginSalt)
{
#ifndef SUPPORT_STORAGE_ID
  return EmptyCString();
#else
  GMP_LOG("CDMStorageIdProvider::ComputeStorageId");

  std::string machineId;
  if (!rlz_lib::GetMachineId(&machineId)) {
    GMP_LOG("CDMStorageIdProvider::ComputeStorageId: get machineId failed.");
    return EmptyCString();
  }

  std::string originSalt(aOriginSalt.BeginReading(), aOriginSalt.Length());
  std::string input = machineId + originSalt + CDMStorageIdProvider::kBrowserIdentifier;
  nsAutoCString storageId;
  nsresult rv;
  nsCOMPtr<nsICryptoHash> hasher = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG("CDMStorageIdProvider::ComputeStorageId: no crypto hash(0x%08" PRIx32 ")",
            static_cast<uint32_t>(rv));
    return EmptyCString();
  }

  rv = hasher->Init(nsICryptoHash::SHA256);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG("CDMStorageIdProvider::ComputeStorageId: failed to initialize hash(0x%08" PRIx32 ")",
            static_cast<uint32_t>(rv));
    return EmptyCString();
  }

  rv = hasher->Update(reinterpret_cast<const uint8_t*>(input.c_str()), input.size());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG("CDMStorageIdProvider::ComputeStorageId: failed to update hash(0x%08" PRIx32 ")",
            static_cast<uint32_t>(rv));
    return EmptyCString();
  }

  rv = hasher->Finish(false, storageId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG("CDMStorageIdProvider::ComputeStorageId: failed to get the final hash result(0x%08" PRIx32 ")",
            static_cast<uint32_t>(rv));
    return EmptyCString();
  }
  return storageId;
#endif
}

} // namespace mozilla