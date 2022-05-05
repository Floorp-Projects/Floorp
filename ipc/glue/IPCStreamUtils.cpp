/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"
#include "mozIRemoteLazyInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/RemoteLazyInputStreamParent.h"
#include "mozilla/Unused.h"
#include "nsNetCID.h"
#include "BackgroundParentImpl.h"
#include "BackgroundChildImpl.h"

using namespace mozilla::dom;

namespace mozilla {
namespace ipc {

namespace {

// These serialization and cleanup functions could be externally exposed.  For
// now, though, keep them private to encourage use of the safer RAII
// AutoIPCStream class.

template <typename M>
bool SerializeInputStreamWithFdsChild(nsIIPCSerializableInputStream* aStream,
                                      IPCStream& aValue, bool aDelayedStart,
                                      M* aManager) {
  MOZ_RELEASE_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  // If you change this size, please also update the payload size in
  // test_reload_large_postdata.html.
  const uint64_t kTooLargeStream = 1024 * 1024;

  uint32_t sizeUsed = 0;
  AutoTArray<FileDescriptor, 4> fds;
  aStream->Serialize(aValue.stream(), fds, aDelayedStart, kTooLargeStream,
                     &sizeUsed, aManager);

  MOZ_ASSERT(sizeUsed <= kTooLargeStream);

  if (aValue.stream().type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }

  MOZ_ASSERT(fds.IsEmpty(), "argument is unused");

  return true;
}

template <typename M>
bool SerializeInputStreamWithFdsParent(nsIIPCSerializableInputStream* aStream,
                                       IPCStream& aValue, bool aDelayedStart,
                                       M* aManager) {
  MOZ_RELEASE_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  const uint64_t kTooLargeStream = 1024 * 1024;

  uint32_t sizeUsed = 0;
  AutoTArray<FileDescriptor, 4> fds;
  aStream->Serialize(aValue.stream(), fds, aDelayedStart, kTooLargeStream,
                     &sizeUsed, aManager);

  MOZ_ASSERT(sizeUsed <= kTooLargeStream);

  if (aValue.stream().type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }

  MOZ_ASSERT(fds.IsEmpty(), "argument is unused");

  return true;
}

template <typename M>
bool SerializeInputStream(nsIInputStream* aStream, IPCStream& aValue,
                          M* aManager, bool aDelayedStart) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  InputStreamParams params;
  InputStreamHelper::SerializeInputStreamAsPipe(aStream, params, aDelayedStart,
                                                aManager);

  if (params.type() == InputStreamParams::T__None) {
    return false;
  }

  aValue.stream() = params;

  return true;
}

template <typename M>
bool SerializeLazyInputStream(nsIInputStream* aStream, IPCStream& aValue,
                              M* aManager) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(XRE_IsParentProcess());

  // avoid creating a loop between processes by accessing the underlying input
  // stream.
  nsCOMPtr<nsIInputStream> stream = aStream;
  if (nsCOMPtr<mozIRemoteLazyInputStream> remoteLazyInputStream =
          do_QueryInterface(stream)) {
    stream = remoteLazyInputStream->GetInternalStream();
    if (NS_WARN_IF(!stream)) {
      return false;
    }
  }

  uint64_t childID = 0;
  if constexpr (std::is_same_v<dom::ContentParent, M>) {
    childID = aManager->ChildID();
  } else if constexpr (std::is_base_of_v<PBackgroundParent, M>) {
    childID = BackgroundParent::GetChildID(aManager);
  }

  int64_t length = -1;
  if (!InputStreamLengthHelper::GetSyncLength(stream, &length)) {
    length = -1;
  }

  nsresult rv = NS_OK;
  RefPtr<RemoteLazyInputStreamParent> actor =
      RemoteLazyInputStreamParent::Create(aStream, length, childID, &rv,
                                          aManager);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!aManager->SendPRemoteLazyInputStreamConstructor(actor, actor->ID(),
                                                       actor->Size())) {
    return false;
  }

  aValue.stream() = RemoteLazyInputStreamParams(actor);

  return true;
}

template <typename M>
bool SerializeInputStreamChild(nsIInputStream* aStream, M* aManager,
                               IPCStream* aValue,
                               Maybe<IPCStream>* aOptionalValue,
                               bool aDelayedStart) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aValue || aOptionalValue);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
      do_QueryInterface(aStream);

  if (serializable) {
    if (aValue) {
      return SerializeInputStreamWithFdsChild(serializable, *aValue,
                                              aDelayedStart, aManager);
    }

    return SerializeInputStreamWithFdsChild(serializable, aOptionalValue->ref(),
                                            aDelayedStart, aManager);
  }

  if (aValue) {
    return SerializeInputStream(aStream, *aValue, aManager, aDelayedStart);
  }

  return SerializeInputStream(aStream, aOptionalValue->ref(), aManager,
                              aDelayedStart);
}

template <typename M>
bool SerializeInputStreamParent(nsIInputStream* aStream, M* aManager,
                                IPCStream* aValue,
                                Maybe<IPCStream>* aOptionalValue,
                                bool aDelayedStart) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aValue || aOptionalValue);

  // When requesting a delayed start stream from the parent process, serialize
  // it as a remote lazy stream to avoid bloating payloads.
  if (aDelayedStart && XRE_IsParentProcess()) {
    if (aValue) {
      return SerializeLazyInputStream(aStream, *aValue, aManager);
    }

    return SerializeLazyInputStream(aStream, aOptionalValue->ref(), aManager);
  }

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
      do_QueryInterface(aStream);

  if (serializable) {
    if (aValue) {
      return SerializeInputStreamWithFdsParent(serializable, *aValue,
                                               aDelayedStart, aManager);
    }

    return SerializeInputStreamWithFdsParent(
        serializable, aOptionalValue->ref(), aDelayedStart, aManager);
  }

  if (aValue) {
    return SerializeInputStream(aStream, *aValue, aManager, aDelayedStart);
  }

  return SerializeInputStream(aStream, aOptionalValue->ref(), aManager,
                              aDelayedStart);
}

// Returns false if the serialization should not proceed. This means that the
// inputStream is null.
bool NormalizeOptionalValue(nsIInputStream* aStream, IPCStream* aValue,
                            Maybe<IPCStream>* aOptionalValue) {
  if (aValue) {
    // if aStream is null, we will crash when serializing.
    return true;
  }

  if (!aStream) {
    aOptionalValue->reset();
    return false;
  }

  aOptionalValue->emplace();
  return true;
}

}  // anonymous namespace

already_AddRefed<nsIInputStream> DeserializeIPCStream(const IPCStream& aValue) {
  nsTArray<FileDescriptor> fds;  // NOTE: Unused, should be removed.
  return InputStreamHelper::DeserializeInputStream(aValue.stream(), fds);
}

already_AddRefed<nsIInputStream> DeserializeIPCStream(
    const Maybe<IPCStream>& aValue) {
  if (aValue.isNothing()) {
    return nullptr;
  }

  return DeserializeIPCStream(aValue.ref());
}

AutoIPCStream::AutoIPCStream(bool aDelayedStart)
    : mOptionalValue(&mInlineValue), mDelayedStart(aDelayedStart) {}

AutoIPCStream::AutoIPCStream(IPCStream& aTarget, bool aDelayedStart)
    : mValue(&aTarget), mDelayedStart(aDelayedStart) {}

AutoIPCStream::AutoIPCStream(Maybe<IPCStream>& aTarget, bool aDelayedStart)
    : mOptionalValue(&aTarget), mDelayedStart(aDelayedStart) {
  mOptionalValue->reset();
}

AutoIPCStream::~AutoIPCStream() { MOZ_ASSERT(mValue || mOptionalValue); }

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              dom::ContentChild* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  if (!SerializeInputStreamChild(aStream, aManager, mValue, mOptionalValue,
                                 mDelayedStart)) {
    MOZ_CRASH("IPCStream creation failed!");
  }

  return true;
}

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              PBackgroundChild* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  BackgroundChildImpl* impl = static_cast<BackgroundChildImpl*>(aManager);
  if (!SerializeInputStreamChild(aStream, impl, mValue, mOptionalValue,
                                 mDelayedStart)) {
    MOZ_CRASH("IPCStream creation failed!");
  }

  return true;
}

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              net::SocketProcessChild* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  if (!SerializeInputStreamChild(aStream, aManager, mValue, mOptionalValue,
                                 mDelayedStart)) {
    MOZ_CRASH("IPCStream creation failed!");
  }

  return true;
}

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              dom::ContentParent* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  if (!SerializeInputStreamParent(aStream, aManager, mValue, mOptionalValue,
                                  mDelayedStart)) {
    return false;
  }

  return true;
}

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              PBackgroundParent* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  BackgroundParentImpl* impl = static_cast<BackgroundParentImpl*>(aManager);
  if (!SerializeInputStreamParent(aStream, impl, mValue, mOptionalValue,
                                  mDelayedStart)) {
    return false;
  }

  return true;
}

bool AutoIPCStream::Serialize(nsIInputStream* aStream,
                              net::SocketProcessParent* aManager) {
  MOZ_ASSERT(aStream || !mValue);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  // If NormalizeOptionalValue returns false, we don't have to proceed.
  if (!NormalizeOptionalValue(aStream, mValue, mOptionalValue)) {
    return true;
  }

  if (!SerializeInputStreamParent(aStream, aManager, mValue, mOptionalValue,
                                  mDelayedStart)) {
    return false;
  }

  return true;
}

bool AutoIPCStream::IsSet() const {
  MOZ_ASSERT(mValue || mOptionalValue);
  if (mValue) {
    return mValue->stream().type() != InputStreamParams::T__None;
  } else {
    return mOptionalValue->isSome() &&
           mOptionalValue->ref().stream().type() != InputStreamParams::T__None;
  }
}

IPCStream& AutoIPCStream::TakeValue() {
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(IsSet());

  mTaken = true;

  if (mValue) {
    return *mValue;
  }

  IPCStream& value = mOptionalValue->ref();
  return value;
}

Maybe<IPCStream>& AutoIPCStream::TakeOptionalValue() {
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!mValue);
  MOZ_ASSERT(mOptionalValue);
  mTaken = true;
  return *mOptionalValue;
}

void IPDLParamTraits<nsIInputStream*>::Write(IPC::MessageWriter* aWriter,
                                             IProtocol* aActor,
                                             nsIInputStream* aParam) {
  auto autoStream = MakeRefPtr<HoldIPCStream>(/* aDelayedStart */ true);

  bool ok = false;
  bool found = false;

  // We can only serialize our nsIInputStream if it's going to be sent over one
  // of the protocols we support, or a protocol which is managed by one of the
  // protocols we support.
  IProtocol* actor = aActor;
  while (!found && actor) {
    switch (actor->GetProtocolId()) {
      case PContentMsgStart:
        if (actor->GetSide() == mozilla::ipc::ParentSide) {
          ok = autoStream->Serialize(
              aParam, static_cast<mozilla::dom::ContentParent*>(actor));
        } else {
          MOZ_RELEASE_ASSERT(actor->GetSide() == mozilla::ipc::ChildSide);
          ok = autoStream->Serialize(
              aParam, static_cast<mozilla::dom::ContentChild*>(actor));
        }
        found = true;
        break;
      case PBackgroundMsgStart:
        if (actor->GetSide() == mozilla::ipc::ParentSide) {
          ok = autoStream->Serialize(
              aParam, static_cast<mozilla::ipc::PBackgroundParent*>(actor));
        } else {
          MOZ_RELEASE_ASSERT(actor->GetSide() == mozilla::ipc::ChildSide);
          ok = autoStream->Serialize(
              aParam, static_cast<mozilla::ipc::PBackgroundChild*>(actor));
        }
        found = true;
        break;
      default:
        break;
    }

    // Try the actor's manager.
    actor = actor->Manager();
  }

  if (!found) {
    aActor->FatalError(
        "Attempt to send nsIInputStream over an unsupported ipdl protocol");
  }
  MOZ_RELEASE_ASSERT(ok, "Failed to serialize nsIInputStream");

  WriteIPDLParam(aWriter, aActor, autoStream->TakeOptionalValue());

  // Dispatch the autoStream to an async runnable, so that we guarantee it
  // outlives this callstack, and doesn't shut down any actors we created
  // until after we've finished sending the current message.
  NS_ProxyRelease("IPDLParamTraits<nsIInputStream*>::Write::autoStream",
                  NS_GetCurrentThread(), autoStream.forget(), true);
}

bool IPDLParamTraits<nsIInputStream*>::Read(IPC::MessageReader* aReader,
                                            IProtocol* aActor,
                                            RefPtr<nsIInputStream>* aResult) {
  mozilla::Maybe<mozilla::ipc::IPCStream> ipcStream;
  if (!ReadIPDLParam(aReader, aActor, &ipcStream)) {
    return false;
  }

  *aResult = mozilla::ipc::DeserializeIPCStream(ipcStream);
  return true;
}

}  // namespace ipc
}  // namespace mozilla
