/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorageParent.h"
#include "GMPParent.h"
#include "gmp-storage.h"
#include "mozilla/Unused.h"

namespace mozilla {

#ifdef LOG
#  undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

GMPStorageParent::GMPStorageParent(const nsACString& aNodeId,
                                   GMPParent* aPlugin)
    : mNodeId(aNodeId), mPlugin(aPlugin), mShutdown(true) {}

nsresult GMPStorageParent::Init() {
  LOGD(("GMPStorageParent[%p]::Init()", this));

  if (NS_WARN_IF(mNodeId.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<GeckoMediaPluginServiceParent> mps(
      GeckoMediaPluginServiceParent::GetSingleton());
  if (NS_WARN_IF(!mps)) {
    return NS_ERROR_FAILURE;
  }

  bool persistent = false;
  if (NS_WARN_IF(
          NS_FAILED(mps->IsPersistentStorageAllowed(mNodeId, &persistent)))) {
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

mozilla::ipc::IPCResult GMPStorageParent::RecvOpen(
    const nsACString& aRecordName) {
  LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s')", this,
        PromiseFlatCString(aRecordName).get()));

  if (mShutdown) {
    // Shutdown is an expected state, so we do not IPC_FAIL.
    return IPC_OK();
  }

  if (mNodeId.EqualsLiteral("null")) {
    // Refuse to open storage if the page is opened from local disk,
    // or shared across origin.
    LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') failed; null nodeId",
          this, PromiseFlatCString(aRecordName).get()));
    Unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return IPC_OK();
  }

  if (aRecordName.IsEmpty()) {
    LOGD((
        "GMPStorageParent[%p]::RecvOpen(record='%s') failed; record name empty",
        this, PromiseFlatCString(aRecordName).get()));
    Unused << SendOpenComplete(aRecordName, GMPGenericErr);
    return IPC_OK();
  }

  if (mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') failed; record in use",
          this, PromiseFlatCString(aRecordName).get()));
    Unused << SendOpenComplete(aRecordName, GMPRecordInUse);
    return IPC_OK();
  }

  auto err = mStorage->Open(aRecordName);
  MOZ_ASSERT(GMP_FAILED(err) || mStorage->IsOpen(aRecordName));
  LOGD(("GMPStorageParent[%p]::RecvOpen(record='%s') complete; rv=%d", this,
        PromiseFlatCString(aRecordName).get(), err));
  Unused << SendOpenComplete(aRecordName, err);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPStorageParent::RecvRead(
    const nsACString& aRecordName) {
  LOGD(("GMPStorageParent[%p]::RecvRead(record='%s')", this,
        PromiseFlatCString(aRecordName).get()));

  if (mShutdown) {
    // Shutdown is an expected state, so we do not IPC_FAIL.
    return IPC_OK();
  }

  nsTArray<uint8_t> data;
  if (!mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvRead(record='%s') failed; record not open",
          this, PromiseFlatCString(aRecordName).get()));
    Unused << SendReadComplete(aRecordName, GMPClosedErr, data);
  } else {
    GMPErr rv = mStorage->Read(aRecordName, data);
    LOGD(
        ("GMPStorageParent[%p]::RecvRead(record='%s') read %zu bytes "
         "rv=%" PRIu32,
         this, PromiseFlatCString(aRecordName).get(), data.Length(),
         static_cast<uint32_t>(rv)));
    Unused << SendReadComplete(aRecordName, rv, data);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPStorageParent::RecvWrite(
    const nsACString& aRecordName, nsTArray<uint8_t>&& aBytes) {
  LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') %zu bytes", this,
        PromiseFlatCString(aRecordName).get(), aBytes.Length()));

  if (mShutdown) {
    // Shutdown is an expected state, so we do not IPC_FAIL.
    return IPC_OK();
  }

  if (!mStorage->IsOpen(aRecordName)) {
    LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') failed record not open",
          this, PromiseFlatCString(aRecordName).get()));
    Unused << SendWriteComplete(aRecordName, GMPClosedErr);
    return IPC_OK();
  }

  if (aBytes.Length() > GMP_MAX_RECORD_SIZE) {
    LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') failed record too big",
          this, PromiseFlatCString(aRecordName).get()));
    Unused << SendWriteComplete(aRecordName, GMPQuotaExceededErr);
    return IPC_OK();
  }

  GMPErr rv = mStorage->Write(aRecordName, aBytes);
  LOGD(("GMPStorageParent[%p]::RecvWrite(record='%s') write complete rv=%d",
        this, PromiseFlatCString(aRecordName).get(), rv));

  Unused << SendWriteComplete(aRecordName, rv);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPStorageParent::RecvClose(
    const nsACString& aRecordName) {
  LOGD(("GMPStorageParent[%p]::RecvClose(record='%s')", this,
        PromiseFlatCString(aRecordName).get()));

  if (mShutdown) {
    return IPC_OK();
  }

  mStorage->Close(aRecordName);

  return IPC_OK();
}

void GMPStorageParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOGD(("GMPStorageParent[%p]::ActorDestroy(reason=%d)", this, aWhy));
  Shutdown();
}

void GMPStorageParent::Shutdown() {
  LOGD(("GMPStorageParent[%p]::Shutdown()", this));

  if (mShutdown) {
    return;
  }
  mShutdown = true;
  Unused << SendShutdown();

  mStorage = nullptr;
}

}  // namespace gmp
}  // namespace mozilla
