/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorageChild.h"
#include "GMPChild.h"
#include "gmp-storage.h"
#include "base/task.h"

#define ON_GMP_THREAD() (mPlugin->GMPMessageLoop() == MessageLoop::current())

#define CALL_ON_GMP_THREAD(_func, ...) \
  do { \
    if (ON_GMP_THREAD()) { \
      _func(__VA_ARGS__); \
    } else { \
      mPlugin->GMPMessageLoop()->PostTask( \
        dont_add_new_uses_of_this::NewRunnableMethod(this, &GMPStorageChild::_func, ##__VA_ARGS__) \
      ); \
    } \
  } while(false)

static nsTArray<uint8_t>
ToArray(const uint8_t* aData, uint32_t aDataSize)
{
  nsTArray<uint8_t> data;
  data.AppendElements(aData, aDataSize);
  return mozilla::Move(data);
}

namespace mozilla {
namespace gmp {

GMPRecordImpl::GMPRecordImpl(GMPStorageChild* aOwner,
                             const nsCString& aName,
                             GMPRecordClient* aClient)
  : mName(aName)
  , mClient(aClient)
  , mOwner(aOwner)
{
}

GMPErr
GMPRecordImpl::Open()
{
  return mOwner->Open(this);
}

void
GMPRecordImpl::OpenComplete(GMPErr aStatus)
{
  mClient->OpenComplete(aStatus);
}

GMPErr
GMPRecordImpl::Read()
{
  return mOwner->Read(this);
}

void
GMPRecordImpl::ReadComplete(GMPErr aStatus,
                            const uint8_t* aBytes,
                            uint32_t aLength)
{
  mClient->ReadComplete(aStatus, aBytes, aLength);
}

GMPErr
GMPRecordImpl::Write(const uint8_t* aData, uint32_t aDataSize)
{
  return mOwner->Write(this, aData, aDataSize);
}

void
GMPRecordImpl::WriteComplete(GMPErr aStatus)
{
  mClient->WriteComplete(aStatus);
}

GMPErr
GMPRecordImpl::Close()
{
  RefPtr<GMPRecordImpl> kungfuDeathGrip(this);
  // Delete our self reference.
  Release();
  mOwner->Close(this->Name());
  return GMPNoErr;
}

GMPStorageChild::GMPStorageChild(GMPChild* aPlugin)
  : mMonitor("GMPStorageChild")
  , mPlugin(aPlugin)
  , mShutdown(false)
{
  MOZ_ASSERT(ON_GMP_THREAD());
}

GMPErr
GMPStorageChild::CreateRecord(const nsCString& aRecordName,
                              GMPRecord** aOutRecord,
                              GMPRecordClient* aClient)
{
  MonitorAutoLock lock(mMonitor);

  if (mShutdown) {
    NS_WARNING("GMPStorage used after it's been shutdown!");
    return GMPClosedErr;
  }

  MOZ_ASSERT(aRecordName.Length() && aOutRecord);

  if (HasRecord(aRecordName)) {
    return GMPRecordInUse;
  }

  RefPtr<GMPRecordImpl> record(new GMPRecordImpl(this, aRecordName, aClient));
  mRecords.Put(aRecordName, record); // Addrefs

  // The GMPRecord holds a self reference until the GMP calls Close() on
  // it. This means the object is always valid (even if neutered) while
  // the GMP expects it to be.
  record.forget(aOutRecord);

  return GMPNoErr;
}

bool
GMPStorageChild::HasRecord(const nsCString& aRecordName)
{
  mMonitor.AssertCurrentThreadOwns();
  return mRecords.Contains(aRecordName);
}

already_AddRefed<GMPRecordImpl>
GMPStorageChild::GetRecord(const nsCString& aRecordName)
{
  MonitorAutoLock lock(mMonitor);
  RefPtr<GMPRecordImpl> record;
  mRecords.Get(aRecordName, getter_AddRefs(record));
  return record.forget();
}

GMPErr
GMPStorageChild::Open(GMPRecordImpl* aRecord)
{
  MonitorAutoLock lock(mMonitor);

  if (mShutdown) {
    NS_WARNING("GMPStorage used after it's been shutdown!");
    return GMPClosedErr;
  }

  if (!HasRecord(aRecord->Name())) {
    // Trying to re-open a record that has already been closed.
    return GMPClosedErr;
  }

  CALL_ON_GMP_THREAD(SendOpen, aRecord->Name());

  return GMPNoErr;
}

GMPErr
GMPStorageChild::Read(GMPRecordImpl* aRecord)
{
  MonitorAutoLock lock(mMonitor);

  if (mShutdown) {
    NS_WARNING("GMPStorage used after it's been shutdown!");
    return GMPClosedErr;
  }

  if (!HasRecord(aRecord->Name())) {
    // Record not opened.
    return GMPClosedErr;
  }

  CALL_ON_GMP_THREAD(SendRead, aRecord->Name());

  return GMPNoErr;
}

GMPErr
GMPStorageChild::Write(GMPRecordImpl* aRecord,
                       const uint8_t* aData,
                       uint32_t aDataSize)
{
  if (aDataSize > GMP_MAX_RECORD_SIZE) {
    return GMPQuotaExceededErr;
  }

  MonitorAutoLock lock(mMonitor);

  if (mShutdown) {
    NS_WARNING("GMPStorage used after it's been shutdown!");
    return GMPClosedErr;
  }

  if (!HasRecord(aRecord->Name())) {
    // Record not opened.
    return GMPClosedErr;
  }

  CALL_ON_GMP_THREAD(SendWrite, aRecord->Name(), ToArray(aData, aDataSize));

  return GMPNoErr;
}

GMPErr
GMPStorageChild::Close(const nsCString& aRecordName)
{
  MonitorAutoLock lock(mMonitor);

  if (!HasRecord(aRecordName)) {
    // Already closed.
    return GMPClosedErr;
  }

  mRecords.Remove(aRecordName);

  if (!mShutdown) {
    CALL_ON_GMP_THREAD(SendClose, aRecordName);
  }

  return GMPNoErr;
}

mozilla::ipc::IPCResult
GMPStorageChild::RecvOpenComplete(const nsCString& aRecordName,
                                  const GMPErr& aStatus)
{
  // We don't need a lock to read |mShutdown| since it is only changed in
  // the GMP thread.
  if (mShutdown) {
    return IPC_OK();
  }
  RefPtr<GMPRecordImpl> record = GetRecord(aRecordName);
  if (!record) {
    // Not fatal.
    return IPC_OK();
  }
  record->OpenComplete(aStatus);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPStorageChild::RecvReadComplete(const nsCString& aRecordName,
                                  const GMPErr& aStatus,
                                  InfallibleTArray<uint8_t>&& aBytes)
{
  if (mShutdown) {
    return IPC_OK();
  }
  RefPtr<GMPRecordImpl> record = GetRecord(aRecordName);
  if (!record) {
    // Not fatal.
    return IPC_OK();
  }
  record->ReadComplete(aStatus, aBytes.Elements(), aBytes.Length());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPStorageChild::RecvWriteComplete(const nsCString& aRecordName,
                                   const GMPErr& aStatus)
{
  if (mShutdown) {
    return IPC_OK();
  }
  RefPtr<GMPRecordImpl> record = GetRecord(aRecordName);
  if (!record) {
    // Not fatal.
    return IPC_OK();
  }
  record->WriteComplete(aStatus);
  return IPC_OK();
}

GMPErr
GMPStorageChild::EnumerateRecords(RecvGMPRecordIteratorPtr aRecvIteratorFunc,
                                  void* aUserArg)
{
  MonitorAutoLock lock(mMonitor);

  if (mShutdown) {
    NS_WARNING("GMPStorage used after it's been shutdown!");
    return GMPClosedErr;
  }

  MOZ_ASSERT(aRecvIteratorFunc);
  mPendingRecordIterators.push(RecordIteratorContext(aRecvIteratorFunc, aUserArg));

  CALL_ON_GMP_THREAD(SendGetRecordNames);

  return GMPNoErr;
}

class GMPRecordIteratorImpl : public GMPRecordIterator {
public:
  explicit GMPRecordIteratorImpl(const InfallibleTArray<nsCString>& aRecordNames)
    : mRecordNames(aRecordNames)
    , mIndex(0)
  {
    mRecordNames.Sort();
  }

  GMPErr GetName(const char** aOutName, uint32_t* aOutNameLength) override {
    if (!aOutName || !aOutNameLength) {
      return GMPInvalidArgErr;
    }
    if (mIndex == mRecordNames.Length()) {
      return GMPEndOfEnumeration;
    }
    *aOutName = mRecordNames[mIndex].get();
    *aOutNameLength = mRecordNames[mIndex].Length();
    return GMPNoErr;
  }

  GMPErr NextRecord() override {
    if (mIndex < mRecordNames.Length()) {
      mIndex++;
    }
    return (mIndex < mRecordNames.Length()) ? GMPNoErr
                                            : GMPEndOfEnumeration;
  }

  void Close() override {
    delete this;
  }

private:
  nsTArray<nsCString> mRecordNames;
  size_t mIndex;
};

mozilla::ipc::IPCResult
GMPStorageChild::RecvRecordNames(InfallibleTArray<nsCString>&& aRecordNames,
                                 const GMPErr& aStatus)
{
  RecordIteratorContext ctx;
  {
    MonitorAutoLock lock(mMonitor);
    if (mShutdown || mPendingRecordIterators.empty()) {
      return IPC_OK();
    }
    ctx = mPendingRecordIterators.front();
    mPendingRecordIterators.pop();
  }

  if (GMP_FAILED(aStatus)) {
    ctx.mFunc(nullptr, ctx.mUserArg, aStatus);
  } else {
    ctx.mFunc(new GMPRecordIteratorImpl(aRecordNames), ctx.mUserArg, GMPNoErr);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPStorageChild::RecvShutdown()
{
  // Block any new storage requests, and thus any messages back to the
  // parent. We don't delete any objects here, as that may invalidate
  // GMPRecord pointers held by the GMP.
  MonitorAutoLock lock(mMonitor);
  mShutdown = true;
  while (!mPendingRecordIterators.empty()) {
    mPendingRecordIterators.pop();
  }
  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla

// avoid redefined macro in unified build
#undef ON_GMP_THREAD
#undef CALL_ON_GMP_THREAD
