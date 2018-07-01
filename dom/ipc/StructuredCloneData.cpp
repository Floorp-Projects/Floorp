/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneData.h"

#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "MainThreadUtils.h"
#include "StructuredCloneTags.h"
#include "jsapi.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {
namespace ipc {

StructuredCloneData::StructuredCloneData()
  : StructuredCloneData(StructuredCloneHolder::TransferringSupported)
{}

StructuredCloneData::StructuredCloneData(StructuredCloneData&& aOther)
  : StructuredCloneData(StructuredCloneHolder::TransferringSupported)
{
  *this = std::move(aOther);
}

StructuredCloneData::StructuredCloneData(TransferringSupport aSupportsTransferring)
  : StructuredCloneHolder(StructuredCloneHolder::CloningSupported,
                          aSupportsTransferring,
                          StructuredCloneHolder::StructuredCloneScope::DifferentProcess)
  , mExternalData(JS::StructuredCloneScope::DifferentProcess)
  , mInitialized(false)
{}

StructuredCloneData::~StructuredCloneData()
{}

StructuredCloneData&
StructuredCloneData::operator=(StructuredCloneData&& aOther)
{
  mExternalData = std::move(aOther.mExternalData);
  mSharedData = std::move(aOther.mSharedData);
  mIPCStreams = std::move(aOther.mIPCStreams);
  mInitialized = aOther.mInitialized;

  return *this;
}

bool
StructuredCloneData::Copy(const StructuredCloneData& aData)
{
  if (!aData.mInitialized) {
    return true;
  }

  if (aData.SharedData()) {
    mSharedData = aData.SharedData();
  } else {
    mSharedData =
      SharedJSAllocatedData::CreateFromExternalData(aData.Data());
    NS_ENSURE_TRUE(mSharedData, false);
  }

  if (mSupportsTransferring) {
    PortIdentifiers().AppendElements(aData.PortIdentifiers());
  }

  MOZ_ASSERT(BlobImpls().IsEmpty());
  BlobImpls().AppendElements(aData.BlobImpls());

  MOZ_ASSERT(GetSurfaces().IsEmpty());
  MOZ_ASSERT(WasmModules().IsEmpty());

  MOZ_ASSERT(InputStreams().IsEmpty());
  InputStreams().AppendElements(aData.InputStreams());

  mInitialized = true;

  return true;
}

void
StructuredCloneData::Read(JSContext* aCx,
                          JS::MutableHandle<JS::Value> aValue,
                          ErrorResult &aRv)
{
  MOZ_ASSERT(mInitialized);

  nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
  MOZ_ASSERT(global);

  ReadFromBuffer(global, aCx, Data(), aValue, aRv);
}

void
StructuredCloneData::Write(JSContext* aCx,
                           JS::Handle<JS::Value> aValue,
                           ErrorResult &aRv)
{
  Write(aCx, aValue, JS::UndefinedHandleValue, aRv);
}

void
StructuredCloneData::Write(JSContext* aCx,
                           JS::Handle<JS::Value> aValue,
                           JS::Handle<JS::Value> aTransfer,
                           ErrorResult &aRv)
{
  MOZ_ASSERT(!mInitialized);

  StructuredCloneHolder::Write(aCx, aValue, aTransfer,
                               JS::CloneDataPolicy().denySharedArrayBuffer(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  JSStructuredCloneData data(mBuffer->scope());
  mBuffer->abandon();
  mBuffer->steal(&data);
  mBuffer = nullptr;
  mSharedData = new SharedJSAllocatedData(std::move(data));
  mInitialized = true;
}

enum ActorFlavorEnum {
  Parent = 0,
  Child,
};

enum ManagerFlavorEnum {
  ContentProtocol = 0,
  BackgroundProtocol
};

template<typename M>
bool
BuildClonedMessageData(M* aManager, StructuredCloneData& aData,
                       ClonedMessageData& aClonedData)
{
  SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  auto iter = aData.Data().Start();
  size_t size = aData.Data().Size();
  bool success;
  buffer.data = aData.Data().Borrow(iter, size, &success);
  if (NS_WARN_IF(!success)) {
    return false;
  }
  if (aData.SupportsTransferring()) {
    aClonedData.identfiers().AppendElements(aData.PortIdentifiers());
  }

  const nsTArray<RefPtr<BlobImpl>>& blobImpls = aData.BlobImpls();

  if (!blobImpls.IsEmpty()) {
    if (NS_WARN_IF(!aClonedData.blobs().SetLength(blobImpls.Length(), fallible))) {
      return false;
    }

    for (uint32_t i = 0; i < blobImpls.Length(); ++i) {
      nsresult rv = IPCBlobUtils::Serialize(blobImpls[i], aManager,
                                            aClonedData.blobs()[i]);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }
    }
  }

  const nsTArray<nsCOMPtr<nsIInputStream>>& inputStreams = aData.InputStreams();
  if (!inputStreams.IsEmpty()) {
    if (NS_WARN_IF(!aData.IPCStreams().SetCapacity(inputStreams.Length(),
                                                   fallible))) {
      return false;
    }

    InfallibleTArray<IPCStream>& streams = aClonedData.inputStreams();
    uint32_t length = inputStreams.Length();
    streams.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      AutoIPCStream* stream = aData.IPCStreams().AppendElement(fallible);
      if (NS_WARN_IF(!stream)) {
        return false;
      }

      if (!stream->Serialize(inputStreams[i], aManager)) {
        return false;
      }
      streams.AppendElement(stream->TakeValue());
    }
  }

  return true;
}

bool
StructuredCloneData::BuildClonedMessageDataForParent(
  nsIContentParent* aParent,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData(aParent, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForChild(
  nsIContentChild* aChild,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData(aChild, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForBackgroundParent(
  PBackgroundParent* aParent,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData(aParent, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForBackgroundChild(
  PBackgroundChild* aChild,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData(aChild, *this, aClonedData);
}

// See the StructuredCloneData class block comment for the meanings of each val.
enum MemoryFlavorEnum {
  BorrowMemory = 0,
  CopyMemory,
  StealMemory
};

template<MemoryFlavorEnum>
struct MemoryTraits
{ };

template<>
struct MemoryTraits<BorrowMemory>
{
  typedef const mozilla::dom::ClonedMessageData ClonedMessageType;

  static void ProvideBuffer(const ClonedMessageData& aClonedData,
                            StructuredCloneData& aData)
  {
    const SerializedStructuredCloneBuffer& buffer = aClonedData.data();
    aData.UseExternalData(buffer.data);
  }
};

template<>
struct MemoryTraits<CopyMemory>
{
  typedef const mozilla::dom::ClonedMessageData ClonedMessageType;

  static void ProvideBuffer(const ClonedMessageData& aClonedData,
                            StructuredCloneData& aData)
  {
    const SerializedStructuredCloneBuffer& buffer = aClonedData.data();
    aData.CopyExternalData(buffer.data);
  }
};

template<>
struct MemoryTraits<StealMemory>
{
  // note: not const!
  typedef mozilla::dom::ClonedMessageData ClonedMessageType;

  static void ProvideBuffer(ClonedMessageData& aClonedData,
                            StructuredCloneData& aData)
  {
    SerializedStructuredCloneBuffer& buffer = aClonedData.data();
    aData.StealExternalData(buffer.data);
  }
};

// Note that there isn't actually a difference between Parent/BackgroundParent
// and Child/BackgroundChild in this implementation.  The calling methods,
// however, do maintain the distinction for code-reading purposes and are backed
// by assertions to enforce there is no misuse.
template<MemoryFlavorEnum MemoryFlavor, ActorFlavorEnum Flavor>
static void
UnpackClonedMessageData(typename MemoryTraits<MemoryFlavor>::ClonedMessageType& aClonedData,
                        StructuredCloneData& aData)
{
  const InfallibleTArray<MessagePortIdentifier>& identifiers = aClonedData.identfiers();

  MemoryTraits<MemoryFlavor>::ProvideBuffer(aClonedData, aData);

  if (aData.SupportsTransferring()) {
    aData.PortIdentifiers().AppendElements(identifiers);
  }

  const nsTArray<IPCBlob>& blobs = aClonedData.blobs();
  if (!blobs.IsEmpty()) {
    uint32_t length = blobs.Length();
    aData.BlobImpls().SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(blobs[i]);
      MOZ_ASSERT(blobImpl);

      aData.BlobImpls().AppendElement(blobImpl);
    }
  }

  const InfallibleTArray<IPCStream>& streams = aClonedData.inputStreams();
  if (!streams.IsEmpty()) {
    uint32_t length = streams.Length();
    aData.InputStreams().SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(streams[i]);
      aData.InputStreams().AppendElement(stream);
    }
  }
}

void
StructuredCloneData::BorrowFromClonedMessageDataForParent(const ClonedMessageData& aClonedData)
{
  // PContent parent is always main thread and actor constraints demand we're
  // likewise on that thread.
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<BorrowMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::BorrowFromClonedMessageDataForChild(const ClonedMessageData& aClonedData)
{
  // PContent child is always main thread and actor constraints demand we're
  // likewise on that thread.
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<BorrowMemory, Child>(aClonedData, *this);
}

void
StructuredCloneData::BorrowFromClonedMessageDataForBackgroundParent(const ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(IsOnBackgroundThread());
  UnpackClonedMessageData<BorrowMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::BorrowFromClonedMessageDataForBackgroundChild(const ClonedMessageData& aClonedData)
{
  // No thread assertion; BackgroundChild can happen on any thread.
  UnpackClonedMessageData<BorrowMemory, Child>(aClonedData, *this);
}

void
StructuredCloneData::CopyFromClonedMessageDataForParent(const ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<CopyMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::CopyFromClonedMessageDataForChild(const ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<CopyMemory, Child>(aClonedData, *this);
}

void
StructuredCloneData::CopyFromClonedMessageDataForBackgroundParent(const ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(IsOnBackgroundThread());
  UnpackClonedMessageData<CopyMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::CopyFromClonedMessageDataForBackgroundChild(const ClonedMessageData& aClonedData)
{
  UnpackClonedMessageData<CopyMemory, Child>(aClonedData, *this);
}

void
StructuredCloneData::StealFromClonedMessageDataForParent(ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<StealMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::StealFromClonedMessageDataForChild(ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(NS_IsMainThread());
  UnpackClonedMessageData<StealMemory, Child>(aClonedData, *this);
}

void
StructuredCloneData::StealFromClonedMessageDataForBackgroundParent(ClonedMessageData& aClonedData)
{
  MOZ_ASSERT(IsOnBackgroundThread());
  UnpackClonedMessageData<StealMemory, Parent>(aClonedData, *this);
}

void
StructuredCloneData::StealFromClonedMessageDataForBackgroundChild(ClonedMessageData& aClonedData)
{
  UnpackClonedMessageData<StealMemory, Child>(aClonedData, *this);
}


void
StructuredCloneData::WriteIPCParams(IPC::Message* aMsg) const
{
  WriteParam(aMsg, Data());
}

bool
StructuredCloneData::ReadIPCParams(const IPC::Message* aMsg,
                                   PickleIterator* aIter)
{
  MOZ_ASSERT(!mInitialized);
  JSStructuredCloneData data(JS::StructuredCloneScope::DifferentProcess);
  if (!ReadParam(aMsg, aIter, &data)) {
    return false;
  }
  mSharedData = new SharedJSAllocatedData(std::move(data));
  mInitialized = true;
  return true;
}

bool
StructuredCloneData::CopyExternalData(const char* aData,
                                      size_t aDataLength)
{
  MOZ_ASSERT(!mInitialized);
  mSharedData = SharedJSAllocatedData::CreateFromExternalData(aData,
                                                              aDataLength);
  NS_ENSURE_TRUE(mSharedData, false);
  mInitialized = true;
  return true;
}

bool
StructuredCloneData::CopyExternalData(const JSStructuredCloneData& aData)
{
  MOZ_ASSERT(!mInitialized);
  mSharedData = SharedJSAllocatedData::CreateFromExternalData(aData);
  NS_ENSURE_TRUE(mSharedData, false);
  mInitialized = true;
  return true;
}

bool
StructuredCloneData::StealExternalData(JSStructuredCloneData& aData)
{
  MOZ_ASSERT(!mInitialized);
  mSharedData = new SharedJSAllocatedData(std::move(aData));
  mInitialized = true;
  return true;
}

already_AddRefed<SharedJSAllocatedData>
StructuredCloneData::TakeSharedData()
{
  return mSharedData.forget();
}

} // namespace ipc
} // namespace dom
} // namespace mozilla
