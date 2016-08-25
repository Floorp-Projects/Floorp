/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorageParent.h"
#include "GMPParent.h"
#include "gmp-storage.h"
#include "mozilla/Unused.h"
#include "mozIGeckoMediaPluginService.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

GMPStorageParent::GMPStorageParent(const nsCString& aNodeId,
                                   GMPParent* aPlugin)
  : mNodeId(aNodeId)
  , mPlugin(aPlugin)
  , mShutdown(true)
{
}

nsresult
GMPStorageParent::Init()
{
  LOGD(("GMPStorageParent[%p]::Init()", this));

  if (NS_WARN_IF(mNodeId.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<GeckoMediaPluginServiceParent> mps(GeckoMediaPluginServiceParent::GetSingleton());
  if (NS_WARN_IF(!mps)) {
    return NS_ERROR_FAILURE;
  }

  bool persistent = false;
  if (NS_WARN_IF(NS_FAILED(mps->IsPersistentStorageAllowed(mNodeId, &persistent)))) {
    return NS_ERROR_FAILURE;
  }
  if (persistent) {
    mStorage = CreateGMPDiskStorage(mNodeId, mPlugin->GetPluginBaseName());
  } else {
    mStorage = mps->GetMemoryStorageFor(mNodeId);
  }
  if (!mStorage) {
    return NS_ERROR_FAILURE;
  }

  mShutdown = false;
  return NS_OK;
}

bool
GMPStorageParent::RecvOpen(const nsCString& aRecordName)
{
  LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s')",
       this, aRecordName.get()));

  if (mShutdown) {
    return false;
  }

  if (mNodeId.EqualsLiteral("null")) {
    // Refuse to open storage if the page is opened from local disk,
    // or shared across origin.
    LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') failed; null nodeId",
          this, aRecordName.get()));
    Unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  if (aRecordName.IsEmpty()) {
    LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') failed; record name empty",
          this, aRecordName.get()));
    Unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return true;
  }

  if (mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') failed; record in use",
          this, aRecordName.get()));
    Unused << SendOpenComplete(aRecordName, GMPRecordInUse);
    return true;
  }

  auto err = mStorage->Open(aRecordName);
  MOZ_ASSERT(GMP_FAILED(err) || mStorage->IsOpen(aRecordName));
  LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') complete; rv=%d",
        this, aRecordName.get(), err));
  Unused << SendOpenComplete(aRecordName, err);

  return true;
}

bool
GMPStorageParent::RecvRead(const nsCString& aRecordName)
{
  LOGD(("GMPStorageParent[%p]::RecvRead(record='%s')",
       this, aRecordName.get()));

  if (mShutdown) {
    return false;
  }

  nsTArray<uint8_t> data;
  if (!mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvRead(record='%s') failed; record not open",
         this, aRecordName.get()));
    Unused << SendReadComplete(aRecordName, GMPClosedErr, data);
  } else {
    GMPErr rv = mStorage->Read(aRecordName, data);
    LOGD(("GMPStorageParent[%p]::RecvRead(record='%s') read %d bytes rv=%d",
      this, aRecordName.get(), data.Length(), rv));
    Unused << SendReadComplete(aRecordName, rv, data);
  }

  return true;
}

bool
GMPStorageParent::RecvWrite(const nsCString& aRecordName,
                            InfallibleTArray<uint8_t>&& aBytes)
{
  LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') %d bytes",
        this, aRecordName.get(), aBytes.Length()));

  if (mShutdown) {
    return false;
  }

  if (!mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') failed record not open",
          this, aRecordName.get()));
    Unused << SendWriteComplete(aRecordName, GMPClosedErr);
    return true;
  }

  if (aBytes.Length() > GMP_MAX_RECORD_SIZE) {
    LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') failed record too big",
          this, aRecordName.get()));
    Unused << SendWriteComplete(aRecordName, GMPQuotaExceededErr);
    return true;
  }

  GMPErr rv = mStorage->Write(aRecordName, aBytes);
  LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') write complete rv=%d",
        this, aRecordName.get(), rv));

  Unused << SendWriteComplete(aRecordName, rv);

  return true;
}

bool
GMPStorageParent::RecvGetRecordNames()
{
  if (mShutdown) {
    return true;
  }

  nsTArray<nsCString> recordNames;
  GMPErr status = mStorage->GetRecordNames(recordNames);

  LOGD(("GMPStorageParent[%p]::RecvGetRecordNames() status=%d numRecords=%d",
        this, status, recordNames.Length()));

  Unused << SendRecordNames(recordNames, status);

  return true;
}

bool
GMPStorageParent::RecvClose(const nsCString& aRecordName)
{
  LOGD(("GMPStorageParent[%p]::RecvClose(record='%s')",
        this, aRecordName.get()));

  if (mShutdown) {
    return true;
  }

  mStorage->Close(aRecordName);

  return true;
}

void
GMPStorageParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("GMPStorageParent[%p]::ActorDestroy(reason=%d)", this, aWhy));
  Shutdown();
}

void
GMPStorageParent::Shutdown()
{
  LOGD(("GMPStorageParent[%p]::Shutdown()", this));

  if (mShutdown) {
    return;
  }
  mShutdown = true;
  Unused << SendShutdown();

  mStorage = nullptr;

}

} // namespace gmp
} // namespace mozilla
