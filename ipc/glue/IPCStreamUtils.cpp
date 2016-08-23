/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/SendStream.h"
#include "mozilla/Unused.h"
#include "nsIAsyncInputStream.h"

namespace mozilla {
namespace ipc {

namespace {

// These serialization and cleanup functions could be externally exposed.  For
// now, though, keep them private to encourage use of the safer RAII
// AutoIPCStream class.

template<typename M>
void
SerializeInputStreamWithFdsChild(nsIInputStream* aStream,
                                 IPCStream& aValue,
                                 M* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  // First attempt simple stream serialization
  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  aValue = InputStreamParamsWithFds();
  InputStreamParamsWithFds& streamWithFds =
    aValue.get_InputStreamParamsWithFds();

  AutoTArray<FileDescriptor, 4> fds;
  serializable->Serialize(streamWithFds.stream(), fds);

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
}

template<typename M>
void
SerializeInputStreamWithFdsParent(nsIInputStream* aStream,
                                  IPCStream& aValue,
                                  M* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  // First attempt simple stream serialization
  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  aValue = InputStreamParamsWithFds();
  InputStreamParamsWithFds& streamWithFds =
    aValue.get_InputStreamParamsWithFds();

  AutoTArray<FileDescriptor, 4> fds;
  serializable->Serialize(streamWithFds.stream(), fds);

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
}

template<typename M>
void
SerializeInputStream(nsIInputStream* aStream, IPCStream& aValue, M* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);

  // First attempt simple stream serialization
  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aStream);
  if (serializable) {
    SerializeInputStreamWithFdsChild(aStream, aValue, aManager);
    return;
  }

  // As a fallback, attempt to stream the data across using a SendStream
  // actor.  This will fail for blocking streams.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aStream);
  aValue = SendStreamChild::Create(asyncStream, aManager);

  if (!aValue.get_PSendStreamChild()) {
    MOZ_CRASH("SendStream creation failed!");
  }
}

template<typename M>
void
SerializeInputStream(nsIInputStream* aStream, OptionalIPCStream& aValue,
                     M* aManager)
{
  if (!aStream) {
    aValue = void_t();
    return;
  }

  aValue = IPCStream();
  SerializeInputStream(aStream, aValue.get_IPCStream(),
                             aManager);
}

void
CleanupIPCStream(IPCStream& aValue, bool aConsumedByIPC)
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

      if (!aConsumedByIPC) {
        Unused << fdSetActor->Send__delete__(fdSetActor);
      }

      // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
      // unconditionally forget them here.  The fds themselves are auto-closed in
      // ~FileDescriptor since they originated in this process.
      fdSetActor->ForgetFileDescriptors(fds);

    } else if (streamWithFds.optionalFds().type() ==
               OptionalFileDescriptorSet::TPFileDescriptorSetParent) {

      AutoTArray<FileDescriptor, 4> fds;

      auto fdSetActor = static_cast<FileDescriptorSetParent*>(
        streamWithFds.optionalFds().get_PFileDescriptorSetParent());
      MOZ_ASSERT(fdSetActor);

      if (!aConsumedByIPC) {
        Unused << fdSetActor->Send__delete__(fdSetActor);
      }

      // FileDescriptorSet doesn't clear its fds in its ActorDestroy, so we
      // unconditionally forget them here.  The fds themselves are auto-closed in
      // ~FileDescriptor since they originated in this process.
      fdSetActor->ForgetFileDescriptors(fds);
    }

    return;
  }

  MOZ_ASSERT(aValue.type() == IPCStream::TPSendStreamChild);

  auto sendStream =
    static_cast<SendStreamChild*>(aValue.get_PSendStreamChild());

  if (!aConsumedByIPC) {
    sendStream->StartDestroy();
    return;
  }

  // If the SendStream was taken to be sent to the parent, then we need to
  // start it before forgetting about it.
  sendStream->Start();
}

void
CleanupIPCStream(OptionalIPCStream& aValue, bool aConsumedByIPC)
{
  if (aValue.type() == OptionalIPCStream::Tvoid_t) {
    return;
  }

  CleanupIPCStream(aValue.get_IPCStream(), aConsumedByIPC);
}

} // anonymous namespace

already_AddRefed<nsIInputStream>
DeserializeIPCStream(const IPCStream& aValue)
{
  if (aValue.type() == IPCStream::TPSendStreamParent) {
    auto sendStream =
      static_cast<SendStreamParent*>(aValue.get_PSendStreamParent());
    return sendStream->TakeReader();
  }

  // Note, we explicitly do not support deserializing the PSendStream actor on
  // the child side.  It can only be sent from child to parent.
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

  return DeserializeInputStream(streamWithFds.stream(), fds);
}

already_AddRefed<nsIInputStream>
DeserializeIPCStream(const OptionalIPCStream& aValue)
{
  if (aValue.type() == OptionalIPCStream::Tvoid_t) {
    return nullptr;
  }

  return DeserializeIPCStream(aValue.get_IPCStream());
}

namespace {

void
AssertValidValueToTake(const IPCStream& aVal)
{
  MOZ_ASSERT(aVal.type() == IPCStream::TPSendStreamChild ||
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

} // anonymous namespace

AutoIPCStream::AutoIPCStream()
  : mInlineValue(void_t())
  , mValue(nullptr)
  , mOptionalValue(&mInlineValue)
  , mTaken(false)
{
}

AutoIPCStream::AutoIPCStream(IPCStream& aTarget)
  : mInlineValue(void_t())
  , mValue(&aTarget)
  , mOptionalValue(nullptr)
  , mTaken(false)
{
}

AutoIPCStream::AutoIPCStream(OptionalIPCStream& aTarget)
  : mInlineValue(void_t())
  , mValue(nullptr)
  , mOptionalValue(&aTarget)
  , mTaken(false)
{
  *mOptionalValue = void_t();
}

AutoIPCStream::~AutoIPCStream()
{
  MOZ_ASSERT(mValue || mOptionalValue);
  if (mValue && IsSet()) {
    CleanupIPCStream(*mValue, mTaken);
  } else {
    CleanupIPCStream(*mOptionalValue, mTaken);
  }
}

void
AutoIPCStream::Serialize(nsIInputStream* aStream, dom::PContentChild* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  if (mValue) {
    SerializeInputStream(aStream, *mValue, aManager);
    AssertValidValueToTake(*mValue);
  } else {
    SerializeInputStream(aStream, *mOptionalValue, aManager);
    AssertValidValueToTake(*mOptionalValue);
  }
}

void
AutoIPCStream::Serialize(nsIInputStream* aStream, PBackgroundChild* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  if (mValue) {
    SerializeInputStream(aStream, *mValue, aManager);
    AssertValidValueToTake(*mValue);
  } else {
    SerializeInputStream(aStream, *mOptionalValue, aManager);
    AssertValidValueToTake(*mOptionalValue);
  }
}

void
AutoIPCStream::Serialize(nsIInputStream* aStream, dom::PContentParent* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  if (mValue) {
    SerializeInputStreamWithFdsParent(aStream, *mValue, aManager);
    AssertValidValueToTake(*mValue);
  } else {
    SerializeInputStreamWithFdsParent(aStream, *mOptionalValue, aManager);
    AssertValidValueToTake(*mOptionalValue);
  }
}

void
AutoIPCStream::Serialize(nsIInputStream* aStream, PBackgroundParent* aManager)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mValue || mOptionalValue);
  MOZ_ASSERT(!mTaken);
  MOZ_ASSERT(!IsSet());

  if (mValue) {
    SerializeInputStreamWithFdsParent(aStream, *mValue, aManager);
    AssertValidValueToTake(*mValue);
  } else {
    SerializeInputStreamWithFdsParent(aStream, *mOptionalValue, aManager);
    AssertValidValueToTake(*mOptionalValue);
  }
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
