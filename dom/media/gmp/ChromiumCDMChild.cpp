/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMChild.h"
#include "GMPContentChild.h"
#include "WidevineUtils.h"
#include "WidevineFileIO.h"
#include "WidevineVideoFrame.h"
#include "GMPLog.h"
#include "GMPPlatform.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include "base/time.h"
#include "GMPUtils.h"
#include "mozilla/ScopeExit.h"
#include "CDMStorageIdProvider.h"
#include "nsReadableUtils.h"

#include <type_traits>

namespace mozilla::gmp {

ChromiumCDMChild::ChromiumCDMChild(GMPContentChild* aPlugin)
    : mPlugin(aPlugin) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild:: ctor this=%p", this);
}

void ChromiumCDMChild::Init(cdm::ContentDecryptionModule_10* aCDM,
                            const nsCString& aStorageId) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  mCDM = aCDM;
  MOZ_ASSERT(mCDM);
  mStorageId = aStorageId;
}

void ChromiumCDMChild::TimerExpired(void* aContext) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::TimerExpired(context=0x%p)", aContext);
  if (mCDM) {
    mCDM->TimerExpired(aContext);
  }
}

class CDMShmemBuffer : public CDMBuffer {
 public:
  CDMShmemBuffer(ChromiumCDMChild* aProtocol, ipc::Shmem aShmem)
      : mProtocol(aProtocol), mSize(aShmem.Size<uint8_t>()), mShmem(aShmem) {
    GMP_LOG_DEBUG("CDMShmemBuffer(size=%" PRIu32 ") created", Size());
    // Note: Chrome initializes the size of a buffer to it capacity. We do the
    // same.
  }

  CDMShmemBuffer(ChromiumCDMChild* aProtocol, ipc::Shmem aShmem,
                 WidevineBuffer* aLocalBuffer)
      : CDMShmemBuffer(aProtocol, aShmem) {
    MOZ_ASSERT(aLocalBuffer->Size() == Size());
    memcpy(Data(), aLocalBuffer->Data(),
           std::min<uint32_t>(aLocalBuffer->Size(), Size()));
  }

  ~CDMShmemBuffer() override {
    GMP_LOG_DEBUG("CDMShmemBuffer(size=%" PRIu32 ") destructed writable=%d",
                  Size(), mShmem.IsWritable());
    if (mShmem.IsWritable()) {
      // The shmem wasn't extracted to send its data back up to the parent
      // process, so we can reuse the shmem.
      mProtocol->GiveBuffer(std::move(mShmem));
    }
  }

  void Destroy() override {
    GMP_LOG_DEBUG("CDMShmemBuffer::Destroy(size=%" PRIu32 ")", Size());
    delete this;
  }
  uint32_t Capacity() const override { return mShmem.Size<uint8_t>(); }

  uint8_t* Data() override { return mShmem.get<uint8_t>(); }

  void SetSize(uint32_t aSize) override {
    MOZ_ASSERT(aSize <= Capacity());
    // Note: We can't use the shmem's size member after ExtractShmem(),
    // has been called, so we track the size exlicitly so that we can use
    // Size() in logging after we've called ExtractShmem().
    GMP_LOG_DEBUG("CDMShmemBuffer::SetSize(size=%" PRIu32 ")", Size());
    mSize = aSize;
  }

  uint32_t Size() const override { return mSize; }

  ipc::Shmem ExtractShmem() {
    ipc::Shmem shmem = mShmem;
    mShmem = ipc::Shmem();
    return shmem;
  }

  CDMShmemBuffer* AsShmemBuffer() override { return this; }

 private:
  RefPtr<ChromiumCDMChild> mProtocol;
  uint32_t mSize;
  mozilla::ipc::Shmem mShmem;
  CDMShmemBuffer(const CDMShmemBuffer&);
  void operator=(const CDMShmemBuffer&);
};

static auto ToString(const nsTArray<ipc::Shmem>& aBuffers) {
  return StringJoin(","_ns, aBuffers, [](auto& s, const ipc::Shmem& shmem) {
    s.AppendInt(static_cast<uint32_t>(shmem.Size<uint8_t>()));
  });
}

cdm::Buffer* ChromiumCDMChild::Allocate(uint32_t aCapacity) {
  GMP_LOG_DEBUG("ChromiumCDMChild::Allocate(capacity=%" PRIu32
                ") bufferSizes={%s}",
                aCapacity, ToString(mBuffers).get());
  MOZ_ASSERT(IsOnMessageLoopThread());

  if (mBuffers.IsEmpty()) {
    Unused << SendIncreaseShmemPoolSize();
  }

  // Find the shmem with the least amount of wasted space if we were to
  // select it for this sized allocation. We need to do this because shmems
  // for decrypted audio as well as video frames are both stored in this
  // list, and we don't want to use the video frame shmems for audio samples.
  const size_t invalid = std::numeric_limits<size_t>::max();
  size_t best = invalid;
  auto wastedSpace = [this, aCapacity](size_t index) {
    return mBuffers[index].Size<uint8_t>() - aCapacity;
  };
  for (size_t i = 0; i < mBuffers.Length(); i++) {
    if (mBuffers[i].Size<uint8_t>() >= aCapacity &&
        (best == invalid || wastedSpace(i) < wastedSpace(best))) {
      best = i;
    }
  }
  if (best == invalid) {
    // The parent process should have bestowed upon us a shmem of appropriate
    // size, but did not! Do a "dive and catch", and create an non-shared
    // memory buffer. The parent will detect this and send us an extra shmem
    // so future frames can be in shmems, i.e. returned on the fast path.
    return new WidevineBuffer(aCapacity);
  }
  ipc::Shmem shmem = mBuffers[best];
  mBuffers.RemoveElementAt(best);
  return new CDMShmemBuffer(this, shmem);
}

void ChromiumCDMChild::SetTimer(int64_t aDelayMs, void* aContext) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::SetTimer(delay=%" PRId64 ", context=0x%p)",
                aDelayMs, aContext);
  RefPtr<ChromiumCDMChild> self(this);
  SetTimerOnMainThread(
      NewGMPTask([self, aContext]() { self->TimerExpired(aContext); }),
      aDelayMs);
}

cdm::Time ChromiumCDMChild::GetCurrentWallTime() {
  return base::Time::Now().ToDoubleT();
}

template <typename MethodType, typename... ParamType>
void ChromiumCDMChild::CallMethod(MethodType aMethod, ParamType&&... aParams) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  // Avoid calling member function after destroy.
  if (!mDestroyed) {
    Unused << (this->*aMethod)(std::forward<ParamType>(aParams)...);
  }
}

template <typename MethodType, typename... ParamType>
void ChromiumCDMChild::CallOnMessageLoopThread(const char* const aName,
                                               MethodType aMethod,
                                               ParamType&&... aParams) {
  if (IsOnMessageLoopThread()) {
    CallMethod(aMethod, std::forward<ParamType>(aParams)...);
  } else {
    auto m = &ChromiumCDMChild::CallMethod<
        decltype(aMethod), const std::remove_reference_t<ParamType>&...>;
    RefPtr<mozilla::Runnable> t =
        NewRunnableMethod<decltype(aMethod),
                          const std::remove_reference_t<ParamType>...>(
            aName, this, m, aMethod, std::forward<ParamType>(aParams)...);
    mPlugin->GMPMessageLoop()->PostTask(t.forget());
  }
}

void ChromiumCDMChild::OnResolveKeyStatusPromise(uint32_t aPromiseId,
                                                 cdm::KeyStatus aKeyStatus) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnResolveKeyStatusPromise(pid=%" PRIu32
                "keystatus=%d)",
                aPromiseId, aKeyStatus);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnResolveKeyStatusPromise",
                          &ChromiumCDMChild::SendOnResolvePromiseWithKeyStatus,
                          aPromiseId, static_cast<uint32_t>(aKeyStatus));
}

bool ChromiumCDMChild::OnResolveNewSessionPromiseInternal(
    uint32_t aPromiseId, const nsCString& aSessionId) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  if (mLoadSessionPromiseIds.Contains(aPromiseId)) {
    // As laid out in the Chromium CDM API, if the CDM fails to load
    // a session it calls OnResolveNewSessionPromise with nullptr as the
    // sessionId. We can safely assume this means that we have failed to load a
    // session as the other methods specify calling 'OnRejectPromise' when they
    // fail.
    bool loadSuccessful = !aSessionId.IsEmpty();
    GMP_LOG_DEBUG(
        "ChromiumCDMChild::OnResolveNewSessionPromise(pid=%u, sid=%s) "
        "resolving %s load session ",
        aPromiseId, aSessionId.get(),
        (loadSuccessful ? "successful" : "failed"));
    mLoadSessionPromiseIds.RemoveElement(aPromiseId);
    return SendResolveLoadSessionPromise(aPromiseId, loadSuccessful);
  }

  return SendOnResolveNewSessionPromise(aPromiseId, aSessionId);
}
void ChromiumCDMChild::OnResolveNewSessionPromise(uint32_t aPromiseId,
                                                  const char* aSessionId,
                                                  uint32_t aSessionIdSize) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnResolveNewSessionPromise(pid=%" PRIu32
                ", sid=%s)",
                aPromiseId, aSessionId);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnResolveNewSessionPromise",
                          &ChromiumCDMChild::OnResolveNewSessionPromiseInternal,
                          aPromiseId, nsCString(aSessionId, aSessionIdSize));
}

void ChromiumCDMChild::OnResolvePromise(uint32_t aPromiseId) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnResolvePromise(pid=%" PRIu32 ")",
                aPromiseId);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnResolvePromise",
                          &ChromiumCDMChild::SendOnResolvePromise, aPromiseId);
}

void ChromiumCDMChild::OnRejectPromise(uint32_t aPromiseId,
                                       cdm::Exception aException,
                                       uint32_t aSystemCode,
                                       const char* aErrorMessage,
                                       uint32_t aErrorMessageSize) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnRejectPromise(pid=%" PRIu32
                ", err=%" PRIu32 " code=%" PRIu32 ", msg='%s')",
                aPromiseId, aException, aSystemCode, aErrorMessage);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnRejectPromise",
                          &ChromiumCDMChild::SendOnRejectPromise, aPromiseId,
                          static_cast<uint32_t>(aException), aSystemCode,
                          nsCString(aErrorMessage, aErrorMessageSize));
}

void ChromiumCDMChild::OnSessionMessage(const char* aSessionId,
                                        uint32_t aSessionIdSize,
                                        cdm::MessageType aMessageType,
                                        const char* aMessage,
                                        uint32_t aMessageSize) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnSessionMessage(sid=%s, type=%" PRIu32
                " size=%" PRIu32 ")",
                aSessionId, aMessageType, aMessageSize);
  CopyableTArray<uint8_t> message;
  message.AppendElements(aMessage, aMessageSize);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnSessionMessage",
                          &ChromiumCDMChild::SendOnSessionMessage,
                          nsCString(aSessionId, aSessionIdSize),
                          static_cast<uint32_t>(aMessageType), message);
}

static auto ToString(const cdm::KeyInformation* aKeysInfo,
                     uint32_t aKeysInfoCount) {
  return StringJoin(","_ns, Span{aKeysInfo, aKeysInfoCount},
                    [](auto& str, const cdm::KeyInformation& key) {
                      str.Append(ToHexString(key.key_id, key.key_id_size));
                      str.AppendLiteral("=");
                      str.AppendInt(key.status);
                    });
}

void ChromiumCDMChild::OnSessionKeysChange(const char* aSessionId,
                                           uint32_t aSessionIdSize,
                                           bool aHasAdditionalUsableKey,
                                           const cdm::KeyInformation* aKeysInfo,
                                           uint32_t aKeysInfoCount) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnSessionKeysChange(sid=%s) keys={%s}",
                aSessionId, ToString(aKeysInfo, aKeysInfoCount).get());

  CopyableTArray<CDMKeyInformation> keys;
  keys.SetCapacity(aKeysInfoCount);
  for (uint32_t i = 0; i < aKeysInfoCount; i++) {
    const cdm::KeyInformation& key = aKeysInfo[i];
    nsTArray<uint8_t> kid;
    kid.AppendElements(key.key_id, key.key_id_size);
    keys.AppendElement(CDMKeyInformation(kid, key.status, key.system_code));
  }
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnSessionMessage",
                          &ChromiumCDMChild::SendOnSessionKeysChange,
                          nsCString(aSessionId, aSessionIdSize), keys);
}

void ChromiumCDMChild::OnExpirationChange(const char* aSessionId,
                                          uint32_t aSessionIdSize,
                                          cdm::Time aNewExpiryTime) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnExpirationChange(sid=%s, time=%lf)",
                aSessionId, aNewExpiryTime);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnExpirationChange",
                          &ChromiumCDMChild::SendOnExpirationChange,
                          nsCString(aSessionId, aSessionIdSize),
                          aNewExpiryTime);
}

void ChromiumCDMChild::OnSessionClosed(const char* aSessionId,
                                       uint32_t aSessionIdSize) {
  GMP_LOG_DEBUG("ChromiumCDMChild::OnSessionClosed(sid=%s)", aSessionId);
  CallOnMessageLoopThread("gmp::ChromiumCDMChild::OnSessionClosed",
                          &ChromiumCDMChild::SendOnSessionClosed,
                          nsCString(aSessionId, aSessionIdSize));
}

void ChromiumCDMChild::QueryOutputProtectionStatus() {
  GMP_LOG_DEBUG("ChromiumCDMChild::QueryOutputProtectionStatus()");
  if (mCDM) {
    mCDM->OnQueryOutputProtectionStatus(cdm::kQuerySucceeded, uint32_t{},
                                        uint32_t{});
  }
}

void ChromiumCDMChild::OnInitialized(bool aSuccess) {
  MOZ_ASSERT(!mInitPromise.IsEmpty(),
             "mInitPromise should exist during init callback!");
  if (!aSuccess) {
    mInitPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  }
  mInitPromise.ResolveIfExists(true, __func__);
}

cdm::FileIO* ChromiumCDMChild::CreateFileIO(cdm::FileIOClient* aClient) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::CreateFileIO()");
  if (!mPersistentStateAllowed) {
    return nullptr;
  }
  return new WidevineFileIO(aClient);
}

void ChromiumCDMChild::RequestStorageId(uint32_t aVersion) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RequestStorageId() aVersion = %u", aVersion);
  // aVersion >= 0x80000000 are reserved.
  if (aVersion >= 0x80000000) {
    mCDM->OnStorageId(aVersion, nullptr, 0);
    return;
  }
  if (aVersion > CDMStorageIdProvider::kCurrentVersion) {
    mCDM->OnStorageId(aVersion, nullptr, 0);
    return;
  }

  mCDM->OnStorageId(CDMStorageIdProvider::kCurrentVersion,
                    !mStorageId.IsEmpty()
                        ? reinterpret_cast<const uint8_t*>(mStorageId.get())
                        : nullptr,
                    mStorageId.Length());
}

ChromiumCDMChild::~ChromiumCDMChild() {
  GMP_LOG_DEBUG("ChromiumCDMChild:: dtor this=%p", this);
}

bool ChromiumCDMChild::IsOnMessageLoopThread() {
  return mPlugin && mPlugin->GMPMessageLoop() == MessageLoop::current();
}

void ChromiumCDMChild::PurgeShmems() {
  for (ipc::Shmem& shmem : mBuffers) {
    DeallocShmem(shmem);
  }
  mBuffers.Clear();
}

ipc::IPCResult ChromiumCDMChild::RecvPurgeShmems() {
  PurgeShmems();
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvInit(
    const bool& aAllowDistinctiveIdentifier, const bool& aAllowPersistentState,
    InitResolver&& aResolver) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMChild::RecvInit(distinctiveId=%s, persistentState=%s)",
      aAllowDistinctiveIdentifier ? "true" : "false",
      aAllowPersistentState ? "true" : "false");
  mPersistentStateAllowed = aAllowPersistentState;

  RefPtr<ChromiumCDMChild::InitPromise> promise = mInitPromise.Ensure(__func__);
  promise->Then(
      mPlugin->GMPMessageLoop()->SerialEventTarget(), __func__,
      [aResolver](bool /* unused */) { aResolver(true); },
      [aResolver](nsresult rv) {
        GMP_LOG_DEBUG(
            "ChromiumCDMChild::RecvInit() init promise rejected with "
            "rv=%" PRIu32,
            static_cast<uint32_t>(rv));
        aResolver(false);
      });

  if (mCDM) {
    // Once the CDM is initialized we expect it to resolve mInitPromise via
    // ChromiumCDMChild::OnInitialized
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState,
                     // We do not yet support hardware secure codecs
                     false);
  } else {
    GMP_LOG_DEBUG(
        "ChromiumCDMChild::RecvInit() mCDM not set! Is GMP shutting down?");
    mInitPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvSetServerCertificate(
    const uint32_t& aPromiseId, nsTArray<uint8_t>&& aServerCert)

{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvSetServerCertificate() certlen=%zu",
                aServerCert.Length());
  if (mCDM) {
    mCDM->SetServerCertificate(aPromiseId, aServerCert.Elements(),
                               aServerCert.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvCreateSessionAndGenerateRequest(
    const uint32_t& aPromiseId, const uint32_t& aSessionType,
    const uint32_t& aInitDataType, nsTArray<uint8_t>&& aInitData) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMChild::RecvCreateSessionAndGenerateRequest("
      "pid=%" PRIu32 ", sessionType=%" PRIu32 ", initDataType=%" PRIu32
      ") initDataLen=%zu",
      aPromiseId, aSessionType, aInitDataType, aInitData.Length());
  MOZ_ASSERT(aSessionType <= cdm::SessionType::kPersistentUsageRecord);
  MOZ_ASSERT(aInitDataType <= cdm::InitDataType::kWebM);
  if (mCDM) {
    mCDM->CreateSessionAndGenerateRequest(
        aPromiseId, static_cast<cdm::SessionType>(aSessionType),
        static_cast<cdm::InitDataType>(aInitDataType), aInitData.Elements(),
        aInitData.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvLoadSession(
    const uint32_t& aPromiseId, const uint32_t& aSessionType,
    const nsCString& aSessionId) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMChild::RecvLoadSession(pid=%u, type=%u, sessionId=%s)",
      aPromiseId, aSessionType, aSessionId.get());
  if (mCDM) {
    mLoadSessionPromiseIds.AppendElement(aPromiseId);
    mCDM->LoadSession(aPromiseId, static_cast<cdm::SessionType>(aSessionType),
                      aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvUpdateSession(
    const uint32_t& aPromiseId, const nsCString& aSessionId,
    nsTArray<uint8_t>&& aResponse) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvUpdateSession(pid=%" PRIu32
                ", sid=%s) responseLen=%zu",
                aPromiseId, aSessionId.get(), aResponse.Length());
  if (mCDM) {
    mCDM->UpdateSession(aPromiseId, aSessionId.get(), aSessionId.Length(),
                        aResponse.Elements(), aResponse.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvCloseSession(
    const uint32_t& aPromiseId, const nsCString& aSessionId) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvCloseSession(pid=%" PRIu32 ", sid=%s)",
                aPromiseId, aSessionId.get());
  if (mCDM) {
    mCDM->CloseSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvRemoveSession(
    const uint32_t& aPromiseId, const nsCString& aSessionId) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvRemoveSession(pid=%" PRIu32 ", sid=%s)",
                aPromiseId, aSessionId.get());
  if (mCDM) {
    mCDM->RemoveSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvGetStatusForPolicy(
    const uint32_t& aPromiseId, const cdm::HdcpVersion& aMinHdcpVersion) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvGetStatusForPolicy(pid=%" PRIu32
                ", MinHdcpVersion=%" PRIu32 ")",
                aPromiseId, static_cast<uint32_t>(aMinHdcpVersion));
  if (mCDM) {
    cdm::Policy policy;
    policy.min_hdcp_version = aMinHdcpVersion;
    mCDM->GetStatusForPolicy(aPromiseId, policy);
  }
  return IPC_OK();
}

static void InitInputBuffer(const CDMInputBuffer& aBuffer,
                            nsTArray<cdm::SubsampleEntry>& aSubSamples,
                            cdm::InputBuffer_2& aInputBuffer) {
  aInputBuffer.data = aBuffer.mData().get<uint8_t>();
  aInputBuffer.data_size = aBuffer.mData().Size<uint8_t>();

  if (aBuffer.mEncryptionScheme() != cdm::EncryptionScheme::kUnencrypted) {
    MOZ_ASSERT(aBuffer.mEncryptionScheme() == cdm::EncryptionScheme::kCenc ||
               aBuffer.mEncryptionScheme() == cdm::EncryptionScheme::kCbcs);
    aInputBuffer.key_id = aBuffer.mKeyId().Elements();
    aInputBuffer.key_id_size = aBuffer.mKeyId().Length();

    aInputBuffer.iv = aBuffer.mIV().Elements();
    aInputBuffer.iv_size = aBuffer.mIV().Length();

    aSubSamples.SetCapacity(aBuffer.mClearBytes().Length());
    for (size_t i = 0; i < aBuffer.mCipherBytes().Length(); i++) {
      aSubSamples.AppendElement(cdm::SubsampleEntry{aBuffer.mClearBytes()[i],
                                                    aBuffer.mCipherBytes()[i]});
    }
    aInputBuffer.subsamples = aSubSamples.Elements();
    aInputBuffer.num_subsamples = aSubSamples.Length();
    aInputBuffer.encryption_scheme = aBuffer.mEncryptionScheme();
  }
  aInputBuffer.pattern.crypt_byte_block = aBuffer.mCryptByteBlock();
  aInputBuffer.pattern.skip_byte_block = aBuffer.mSkipByteBlock();
  aInputBuffer.timestamp = aBuffer.mTimestamp();
}

bool ChromiumCDMChild::HasShmemOfSize(size_t aSize) const {
  for (const ipc::Shmem& shmem : mBuffers) {
    if (shmem.Size<uint8_t>() == aSize) {
      return true;
    }
  }
  return false;
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvDecrypt(
    const uint32_t& aId, const CDMInputBuffer& aBuffer) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDecrypt()");

  // Parent should have already gifted us a shmem to use as output.
  size_t outputShmemSize = aBuffer.mData().Size<uint8_t>();
  MOZ_ASSERT(HasShmemOfSize(outputShmemSize));

  // Ensure we deallocate the shmem used to send input.
  RefPtr<ChromiumCDMChild> self = this;
  auto autoDeallocateInputShmem =
      MakeScopeExit([&, self] { self->DeallocShmem(aBuffer.mData()); });

  // On failure, we need to ensure that the shmem that the parent sent
  // for the CDM to use to return output back to the parent is deallocated.
  // Otherwise, it will leak.
  auto autoDeallocateOutputShmem = MakeScopeExit([self, outputShmemSize] {
    self->mBuffers.RemoveElementsBy(
        [outputShmemSize, self](ipc::Shmem& aShmem) {
          if (aShmem.Size<uint8_t>() != outputShmemSize) {
            return false;
          }
          self->DeallocShmem(aShmem);
          return true;
        });
  });

  if (!mCDM) {
    GMP_LOG_DEBUG("ChromiumCDMChild::RecvDecrypt() no CDM");
    Unused << SendDecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }
  if (aBuffer.mClearBytes().Length() != aBuffer.mCipherBytes().Length()) {
    GMP_LOG_DEBUG(
        "ChromiumCDMChild::RecvDecrypt() clear/cipher bytes length doesn't "
        "match");
    Unused << SendDecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }

  cdm::InputBuffer_2 input = {};
  nsTArray<cdm::SubsampleEntry> subsamples;
  InitInputBuffer(aBuffer, subsamples, input);

  WidevineDecryptedBlock output;
  cdm::Status status = mCDM->Decrypt(input, &output);

  // CDM should have allocated a cdm::Buffer for output.
  CDMShmemBuffer* buffer =
      output.DecryptedBuffer()
          ? static_cast<CDMShmemBuffer*>(output.DecryptedBuffer())
          : nullptr;
  MOZ_ASSERT_IF(buffer, buffer->AsShmemBuffer());
  if (status != cdm::kSuccess || !buffer) {
    Unused << SendDecryptFailed(aId, status);
    return IPC_OK();
  }

  // Success! Return the decrypted sample to parent.
  MOZ_ASSERT(!HasShmemOfSize(outputShmemSize));
  ipc::Shmem shmem = buffer->ExtractShmem();
  if (SendDecrypted(aId, cdm::kSuccess, std::move(shmem))) {
    // No need to deallocate the output shmem; it should have been returned
    // to the content process.
    autoDeallocateOutputShmem.release();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvInitializeVideoDecoder(
    const CDMVideoDecoderConfig& aConfig) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  MOZ_ASSERT(!mDecoderInitialized);
  if (!mCDM) {
    GMP_LOG_DEBUG("ChromiumCDMChild::RecvInitializeVideoDecoder() no CDM");
    Unused << SendOnDecoderInitDone(cdm::kInitializationError);
    return IPC_OK();
  }
  cdm::VideoDecoderConfig_2 config = {};
  config.codec = static_cast<cdm::VideoCodec>(aConfig.mCodec());
  config.profile = static_cast<cdm::VideoCodecProfile>(aConfig.mProfile());
  config.format = static_cast<cdm::VideoFormat>(aConfig.mFormat());
  config.coded_size =
      mCodedSize = {aConfig.mImageWidth(), aConfig.mImageHeight()};
  nsTArray<uint8_t> extraData(aConfig.mExtraData().Clone());
  config.extra_data = extraData.Elements();
  config.extra_data_size = extraData.Length();
  config.encryption_scheme = aConfig.mEncryptionScheme();
  cdm::Status status = mCDM->InitializeVideoDecoder(config);
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvInitializeVideoDecoder() status=%u",
                status);
  Unused << SendOnDecoderInitDone(status);
  mDecoderInitialized = status == cdm::kSuccess;
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvDeinitializeVideoDecoder() {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDeinitializeVideoDecoder()");
  MOZ_ASSERT(mDecoderInitialized);
  if (mDecoderInitialized && mCDM) {
    mCDM->DeinitializeDecoder(cdm::kStreamTypeVideo);
  }
  mDecoderInitialized = false;
  PurgeShmems();
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvResetVideoDecoder() {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvResetVideoDecoder()");
  if (mDecoderInitialized && mCDM) {
    mCDM->ResetDecoder(cdm::kStreamTypeVideo);
  }
  Unused << SendResetVideoDecoderComplete();
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvDecryptAndDecodeFrame(
    const CDMInputBuffer& aBuffer) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDecryptAndDecodeFrame() t=%" PRId64 ")",
                aBuffer.mTimestamp());
  MOZ_ASSERT(mDecoderInitialized);

  if (!mCDM) {
    GMP_LOG_DEBUG("ChromiumCDMChild::RecvDecryptAndDecodeFrame() no CDM");
    Unused << SendDecodeFailed(cdm::kDecodeError);
    return IPC_OK();
  }

  RefPtr<ChromiumCDMChild> self = this;
  auto autoDeallocateShmem =
      MakeScopeExit([&, self] { self->DeallocShmem(aBuffer.mData()); });

  // The output frame may not have the same timestamp as the frame we put in.
  // We may need to input a number of frames before we receive output. The
  // CDM's decoder reorders to ensure frames output are in presentation order.
  // So we need to store the durations of the frames input, and retrieve them
  // on output.
  mFrameDurations.Insert(aBuffer.mTimestamp(), aBuffer.mDuration());

  cdm::InputBuffer_2 input = {};
  nsTArray<cdm::SubsampleEntry> subsamples;
  InitInputBuffer(aBuffer, subsamples, input);

  WidevineVideoFrame frame;
  cdm::Status rv = mCDM->DecryptAndDecodeFrame(input, &frame);
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDecryptAndDecodeFrame() t=%" PRId64
                " CDM decoder rv=%d",
                aBuffer.mTimestamp(), rv);

  switch (rv) {
    case cdm::kNeedMoreData:
      Unused << SendDecodeFailed(rv);
      break;
    case cdm::kNoKey:
      GMP_LOG_DEBUG("NoKey for sample at time=%" PRId64 "!", input.timestamp);
      // Somehow our key became unusable. Typically this would happen when
      // a stream requires output protection, and the configuration changed
      // such that output protection is no longer available. For example, a
      // non-compliant monitor was attached. The JS player should notice the
      // key status changing to "output-restricted", and is supposed to switch
      // to a stream that doesn't require OP. In order to keep the playback
      // pipeline rolling, just output a black frame. See bug 1343140.
      if (!frame.InitToBlack(mCodedSize.width, mCodedSize.height,
                             input.timestamp)) {
        Unused << SendDecodeFailed(cdm::kDecodeError);
        break;
      }
      [[fallthrough]];
    case cdm::kSuccess:
      if (frame.FrameBuffer()) {
        ReturnOutput(frame);
        break;
      }
      // CDM didn't set a frame buffer on the sample, report it as an error.
      [[fallthrough]];
    default:
      Unused << SendDecodeFailed(rv);
      break;
  }

  return IPC_OK();
}

void ChromiumCDMChild::ReturnOutput(WidevineVideoFrame& aFrame) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  MOZ_ASSERT(aFrame.FrameBuffer());
  gmp::CDMVideoFrame output;
  output.mFormat() = static_cast<cdm::VideoFormat>(aFrame.Format());
  output.mImageWidth() = aFrame.Size().width;
  output.mImageHeight() = aFrame.Size().height;
  output.mYPlane() = {aFrame.PlaneOffset(cdm::VideoPlane::kYPlane),
                      aFrame.Stride(cdm::VideoPlane::kYPlane)};
  output.mUPlane() = {aFrame.PlaneOffset(cdm::VideoPlane::kUPlane),
                      aFrame.Stride(cdm::VideoPlane::kUPlane)};
  output.mVPlane() = {aFrame.PlaneOffset(cdm::VideoPlane::kVPlane),
                      aFrame.Stride(cdm::VideoPlane::kVPlane)};
  output.mTimestamp() = aFrame.Timestamp();

  uint64_t duration = 0;
  if (mFrameDurations.Find(aFrame.Timestamp(), duration)) {
    output.mDuration() = duration;
  }

  CDMBuffer* base = reinterpret_cast<CDMBuffer*>(aFrame.FrameBuffer());
  if (base->AsShmemBuffer()) {
    ipc::Shmem shmem = base->AsShmemBuffer()->ExtractShmem();
    Unused << SendDecodedShmem(output, std::move(shmem));
  } else {
    MOZ_ASSERT(base->AsArrayBuffer());
    Unused << SendDecodedData(output, base->AsArrayBuffer()->ExtractBuffer());
  }
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvDrain() {
  MOZ_ASSERT(IsOnMessageLoopThread());
  if (!mCDM) {
    GMP_LOG_DEBUG("ChromiumCDMChild::RecvDrain() no CDM");
    Unused << SendDrainComplete();
    return IPC_OK();
  }
  WidevineVideoFrame frame;
  cdm::InputBuffer_2 sample = {};
  cdm::Status rv = mCDM->DecryptAndDecodeFrame(sample, &frame);
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDrain();  DecryptAndDecodeFrame() rv=%d",
                rv);
  if (rv == cdm::kSuccess) {
    MOZ_ASSERT(frame.Format() != cdm::kUnknownVideoFormat);
    ReturnOutput(frame);
  } else {
    Unused << SendDrainComplete();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvDestroy() {
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG_DEBUG("ChromiumCDMChild::RecvDestroy()");

  MOZ_ASSERT(!mDecoderInitialized);

  mInitPromise.RejectIfExists(NS_ERROR_ABORT, __func__);

  if (mCDM) {
    mCDM->Destroy();
    mCDM = nullptr;
  }
  mDestroyed = true;

  Unused << Send__delete__(this);

  return IPC_OK();
}

mozilla::ipc::IPCResult ChromiumCDMChild::RecvGiveBuffer(ipc::Shmem&& aBuffer) {
  MOZ_ASSERT(IsOnMessageLoopThread());

  GiveBuffer(std::move(aBuffer));
  return IPC_OK();
}

void ChromiumCDMChild::GiveBuffer(ipc::Shmem&& aBuffer) {
  MOZ_ASSERT(IsOnMessageLoopThread());
  size_t sz = aBuffer.Size<uint8_t>();
  mBuffers.AppendElement(std::move(aBuffer));
  GMP_LOG_DEBUG(
      "ChromiumCDMChild::RecvGiveBuffer(capacity=%zu"
      ") bufferSizes={%s} mDecoderInitialized=%d",
      sz, ToString(mBuffers).get(), mDecoderInitialized);
}

}  // namespace mozilla::gmp
