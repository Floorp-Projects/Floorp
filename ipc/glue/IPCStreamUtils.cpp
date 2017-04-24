/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"
#include "nsNetCID.h"
#include "nsStreamUtils.h"

using namespace mozilla::dom;

namespace mozilla {
namespace ipc {

namespace {

void
AssertValidValueToTake(const IPCStream& aVal)
{
  MOZ_ASSERT(aVal.type() == IPCStream::TIPCRemoteStream ||
             aVal.type() == IPCStream::TInputStreamParamsWithFds);
}

void
AssertValidValueToTake(const OptionalIPCStream& aVal)
{
  MOZ_ASSERT(aVal.type() == OptionalIPCStream::Tvoid_t ||
             aVal.type() == OptionalIPCStream::TIPCStream);
  if (aVal.type() == OptionalIPCStream::TIPCStream) {
    AssertValidValueToTake(aVal.get_IPCStream());
  }
}

// These serialization and cleanup functions could be externally exposed.  For
// now, though, keep them private to encourage use of the safer RAII
// AutoIPCStream class.

template<typename M>
bool
SerializeInputStreamWithFdsChild(nsIIPCSerializableInputStream* aStream,
                                 IPCStream& aValue,
                                 M* aManager)
{
  MOZ_RELEASE_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  aValue = InputStreamParamsWithFds();
  InputStreamParamsWithFds& streamWithFds =
    aValue.get_InputStreamParamsWithFds();

  AutoTArray<FileDescriptor, 4> fds;
  aStream->Serialize(streamWithFds.stream(), fds);

  if (streamWithFds.stream().type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }

  if (fds.IsEmpty()) {
    streamWithFds.optionalFds() = void_t();
  } else {
    PFileDescriptorSetChild* fdSet =
      aManager->SendPFileDescriptorSetConstructor(fds[0]);
    for (uint32_t i = 1; i < fds.Length(); ++i) {
      Unused << fdSet->SendAddFileDescriptor(fds[i]);
    }

    streamWithFds.optionalFds() = fdSet;
  }

  return true;
}

template<typename M>
bool
SerializeInputStreamWithFdsParent(nsIIPCSerializableInputStream* aStream,
                                  IPCStream& aValue,
                                  M* aManager)
{
  MOZ_RELEASE_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  aValue = InputStreamParamsWithFds();
  InputStreamParamsWithFds& streamWithFds =
    aValue.get_InputStreamParamsWithFds();

  AutoTArray<FileDescriptor, 4> fds;
  aStream->Serialize(streamWithFds.stream(), fds);

  if (streamWithFds.stream().type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }

  streamWithFds.optionalFds() = void_t();
  if (!fds.IsEmpty()) {
    PFileDescriptorSetParent* fdSet =
      aManager->SendPFileDescriptorSetConstructor(fds[0]);
    for (uint32_t i = 1; i < fds.Length(); ++i) {
      if (NS_WARN_IF(!fdSet->SendAddFileDescriptor(fds[i]))) {
        Unused << PFileDescriptorSetParent::Send__delete__(fdSet);
        fdSet = nullptr;
        break;
      }
    }

    if (fdSet) {
      streamWithFds.optionalFds() = fdSet;
    }
  }

  return true;
}

template<typename M>
bool
SerializeInputStream(nsIInputStream* aStream, IPCStream& aValue, M* aManager,
                     bool aDelayedStart)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  // As a fallback, attempt to stream the data across using a IPCStream
  // actor. For blocking streams, create a nonblocking pipe instead,
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aStream);
  if (!asyncStream) {
    const uint32_t kBufferSize = 32768; // matches IPCStream buffer size.
    nsCOMPtr<nsIAsyncOutputStream> sink;
    nsresult rv = NS_NewPipe2(getter_AddRefs(asyncStream),
                              getter_AddRefs(sink),
                              true,
                              false,
                              kBufferSize,
                              UINT32_MAX);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

    rv = NS_AsyncCopy(aStream, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                      kBufferSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  }

  MOZ_ASSERT(asyncStream);

  IPCRemoteStream remoteStream;
  remoteStream.delayedStart() = aDelayedStart;
  remoteStream.stream() = IPCStreamSource::Create(asyncStream, aManager);
  aValue = remoteStream;

  return true;
}

template<typename M>
bool
SerializeInputStreamChild(nsIInputStream* aStream, M* aManager,
                          IPCStream* aValue,
                          OptionalIPCStream* aOptionalValue,
                          bool aDelayedStart)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aValue || aOptionalValue);

  // If a stream is known to be larger than 1MB, prefer sending it in chunks.
  const uint64_t kTooLargeStream = 1024 * 1024;

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aStream);

  // ExpectedSerializedLength() returns the length of the stream if serialized.
  // This is useful to decide if we want to continue using the serialization
  // directly, or if it's better to use IPCStream.
  uint64_t expectedLength =
    serializable ? serializable->ExpectedSerializedLength().valueOr(0) : 0;
  if (serializable && expectedLength < kTooLargeStream) {
    if (aValue) {
      return SerializeInputStreamWithFdsChild(serializable, *aValue, aManager);
    }

    return SerializeInputStreamWithFdsChild(serializable, *aOptionalValue,
                                            aManager);
  }

  if (aValue) {
    return SerializeInputStream(aStream, *aValue, aManager, aDelayedStart);
  }

  return SerializeInputStream(aStream, *aOptionalValue, aManager, aDelayedStart);
}

template<typename M>
bool
SerializeInputStreamParent(nsIInputStream* aStream, M* aManager,
                           IPCStream* aValue,
                           OptionalIPCStream* aOptionalValue,
                           bool aDelayedStart)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aValue || aOptionalValue);

  // If a stream is known to be larger than 1MB, prefer sending it in chunks.
  const uint64_t kTooLargeStream = 1024 * 1024;

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aStream);
  uint64_t expectedLength =
    serializable ? serializable->ExpectedSerializedLength().valueOr(0) : 0;

  if (serializable && expectedLength < kTooLargeStream) {
    if (aValue) {
      return SerializeInputStreamWithFdsParent(serializable, *aValue, aManager);
    }

    return SerializeInputStreamWithFdsParent(serializable, *aOptionalValue,
                                             aManager);
  }

  if (aValue) {
    return SerializeInputStream(aStream, *aValue, aManager, aDelayedStart);
  }

  return SerializeInputStream(aStream, *aOptionalValue, aManager, aDelayedStart);
}

void
CleanupIPCStream(IPCStream& aValue, bool aConsumedByIPC, bool aDelayedStart)
{
  if (aValue.type() == IPCStream::T__None) {
    return;
  }

  if (aValue.type() == IPCStream::TInputStreamParamsWithFds) {

    InputStreamParamsWithFds& streamWithFds =
      aValue.get_InputStreamParamsWithFds();

    // Cleanup file descriptors if necessary
    if (streamWithFds.optionalFds().type() ==
        OptionalFileDescriptorSet::TPFileDescriptorSetChild) {

      AutoTArray<FileDescriptor, 4> fds;

      auto fdSetActor = static_cast<FileDescriptorSetChild*>(
        streamWithFds.optionalFds().get_PFileDescriptorSetChild());
      MOZ_ASSERT(fdSetActor);

      // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
      // unconditionally forget them here.  The fds themselves are auto-closed in
      // ~FileDescriptor since they originated in this process.
      fdSetActor->ForgetFileDescriptors(fds);

      if (!aConsumedByIPC) {
        Unused << fdSetActor->Send__delete__(fdSetActor);
      }

    } else if (streamWithFds.optionalFds().type() ==
               OptionalFileDescriptorSet::TPFileDescriptorSetParent) {

      AutoTArray<FileDescriptor, 4> fds;

      auto fdSetActor = static_cast<FileDescriptorSetParent*>(
        streamWithFds.optionalFds().get_PFileDescriptorSetParent());
      MOZ_ASSERT(fdSetActor);

      // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
      // unconditionally forget them here.  The fds themselves are auto-closed in
      // ~FileDescriptor since they originated in this process.
      fdSetActor->ForgetFileDescriptors(fds);

      if (!aConsumedByIPC) {
        Unused << fdSetActor->Send__delete__(fdSetActor);
      }
    }

    return;
  }

  MOZ_ASSERT(aValue.type() == IPCStream::TIPCRemoteStream);
  IPCRemoteStreamType& remoteInputStream =
    aValue.get_IPCRemoteStream().stream();

  IPCStreamSource* source = nullptr;
  if (remoteInputStream.type() == IPCRemoteStreamType::TPChildToParentStreamChild) {
    source = IPCStreamSource::Cast(remoteInputStream.get_PChildToParentStreamChild());
  } else {
    MOZ_ASSERT(remoteInputStream.type() == IPCRemoteStreamType::TPParentToChildStreamParent);
    source = IPCStreamSource::Cast(remoteInputStream.get_PParentToChildStreamParent());
  }

  MOZ_ASSERT(source);

  // If the source stream has not been taken to be sent to the other side, we
  // can destroy it.
  if (!aConsumedByIPC) {
    source->StartDestroy();
    return;
  }

  if (!aDelayedStart) {
    // If we don't need to do a delayedStart, we start it now. Otherwise, the
    // Start() will be called at the first use by the
    // IPCStreamDestination::DelayedStartInputStream.
    source->Start();
  }
}

void
CleanupIPCStream(OptionalIPCStream& aValue, bool aConsumedByIPC, bool aDelayedStart)
{
  if (aValue.type() == OptionalIPCStream::Tvoid_t) {
    return;
  }

  CleanupIPCStream(aValue.get_IPCStream(), aConsumedByIPC, aDelayedStart);
}

// Returns false if the serialization should not proceed. This means that the
// inputStream is null.
bool
NormalizeOptionalValue(nsIInputStream* aStream,
                       IPCStream* aValue,
                       OptionalIPCStream* aOptionalValue)
{
  if (aValue) {
    // if aStream is null, we will crash when serializing.
    return true;
  }

  if (!aStream) {
    *aOptionalValue = void_t();
    return false;
  }

  *aOptionalValue = IPCStream();
  return true;
}

} // anonymous namespace

already_AddRefed<nsIInputStream>
DeserializeIPCStream(const IPCStream& aValue)
{
  if (aValue.type() == IPCStream::TIPCRemoteStream) {
    const IPCRemoteStream& remoteStream = aValue.get_IPCRemoteStream();
    const IPCRemoteStreamType& remoteStreamType = remoteStream.stream();
    IPCStreamDestination* destinationStream;

    if (remoteStreamType.type() == IPCRemoteStreamType::TPChildToParentStreamParent) {
      destinationStream =
        IPCStreamDestination::Cast(remoteStreamType.get_PChildToParentStreamParent());
    } else {
      MOZ_ASSERT(remoteStreamType.type() == IPCRemoteStreamType::TPParentToChildStreamChild);
      destinationStream =
        IPCStreamDestination::Cast(remoteStreamType.get_PParentToChildStreamChild());
    }

    destinationStream->SetDelayedStart(remoteStream.delayedStart());
    return destinationStream->TakeReader();
  }

  // Note, we explicitly do not support deserializing the PChildToParentStream actor on
  // the child side nor the PParentToChildStream actor on the parent side.
  MOZ_ASSERT(aValue.type() == IPCStream::TInputStreamParamsWithFds);

  const InputStreamParamsWithFds& streamWithFds =
    aValue.get_InputStreamParamsWithFds();

  AutoTArray<FileDescriptor, 4> fds;
  if (streamWithFds.optionalFds().type() ==
      OptionalFileDescriptorSet::TPFileDescriptorSetParent) {

    auto fdSetActor = static_cast<FileDescriptorSetParent*>(
      streamWithFds.optionalFds().get_PFileDescriptorSetParent());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    if (!fdSetActor->Send__delete__(fdSetActor)) {
      // child process is gone, warn and allow actor to clean up normally
      NS_WARNING("Failed to delete fd set actor.");
    }
  } else if (streamWithFds.optionalFds().type() ==
             OptionalFileDescriptorSet::TPFileDescriptorSetChild) {

    auto fdSetActor = static_cast<FileDescriptorSetChild*>(
      streamWithFds.optionalFds().get_PFileDescriptorSetChild());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    Unused << fdSetActor->Send__delete__(fdSetActor);
  }

  return InputStreamHelper::DeserializeInputStream(streamWithFds.stream(), fds);
}

already_AddRefed<nsIInputStream>
DeserializeIPCStream(const OptionalIPCStream& aValue)
{
  if (aValue.type() == OptionalIPCStream::Tvoid_t) {
    return nullptr;
  }

  return DeserializeIPCStream(aValue.get_IPCStream());
}

AutoIPCStream::AutoIPCStream(bool aDelayedStart)
  : mInlineValue(void_t())
  , mValue(nullptr)
  , mOptionalValue(&mInlineValue)
  , mTaken(false)
  , mDelayedStart(aDelayedStart)
{
}

AutoIPCStream::AutoIPCStream(IPCStream& aTarget, bool aDelayedStart)
  : mInlineValue(void_t())
  , mValue(&aTarget)
  , mOptionalValue(nullptr)
  , mTaken(false)
  , mDelayedStart(aDelayedStart)
{
}

AutoIPCStream::AutoIPCStream(OptionalIPCStream& aTarget, bool aDelayedStart)
  : mInlineValue(void_t())
  , mValue(nullptr)
  , mOptionalValue(&aTarget)
  , mTaken(false)
  , mDelayedStart(aDelayedStart)
{
  *mOptionalValue = void_t();
}

AutoIPCStream::~AutoIPCStream()
{
  MOZ_ASSERT(mValue || mOptionalValue);
  if (mValue && IsSet()) {
    CleanupIPCStream(*mValue, mTaken, mDelayedStart);
  } else {
    CleanupIPCStream(*mOptionalValue, mTaken, mDelayedStart);
  }
}

bool
AutoIPCStream::Serialize(nsIInputStream* aStream, dom::nsIContentChild* aManager)
{
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

  if (mValue) {
    AssertValidValueToTake(*mValue);
  } else {
    AssertValidValueToTake(*mOptionalValue);
  }

  return true;
}

bool
AutoIPCStream::Serialize(nsIInputStream* aStream, PBackgroundChild* aManager)
{
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

  if (mValue) {
    AssertValidValueToTake(*mValue);
  } else {
    AssertValidValueToTake(*mOptionalValue);
  }

  return true;
}

bool
AutoIPCStream::Serialize(nsIInputStream* aStream,
                         dom::nsIContentParent* aManager)
{
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

  if (mValue) {
    AssertValidValueToTake(*mValue);
  } else {
    AssertValidValueToTake(*mOptionalValue);
  }

  return true;
}

bool
AutoIPCStream::Serialize(nsIInputStream* aStream, PBackgroundParent* aManager)
{
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

  if (mValue) {
    AssertValidValueToTake(*mValue);
  } else {
    AssertValidValueToTake(*mOptionalValue);
  }

  return true;
}

bool
AutoIPCStream::IsSet() const
{
  MOZ_ASSERT(mValue || mOptionalValue);
  if (mValue) {
    return mValue->type() != IPCStream::T__None;
  } else {
    return mOptionalValue->type() != OptionalIPCStream::Tvoid_t &&
           mOptionalValue->get_IPCStream().type() != IPCStream::T__None;
  }
}

IPCStream&
AutoIPCStream::TakeValue()
{
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(IsSet());

  mTaken = true;

  if (mValue) {
    AssertValidValueToTake(*mValue);
    return *mValue;
  }

  IPCStream& value =
    mOptionalValue->get_IPCStream();

  AssertValidValueToTake(value);
  return value;
}

OptionalIPCStream&
AutoIPCStream::TakeOptionalValue()
{
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!mValue);
  MOZ_ASSERT(mOptionalValue);
  mTaken = true;
  AssertValidValueToTake(*mOptionalValue);
  return *mOptionalValue;
}

} // namespace ipc
} // namespace mozilla
