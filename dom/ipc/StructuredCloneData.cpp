/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneData.h"

#include "nsIDOMDOMException.h"
#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "MainThreadUtils.h"
#include "StructuredCloneTags.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {
namespace ipc {

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

  JSStructuredCloneData data;
  mBuffer->abandon();
  mBuffer->steal(&data);
  mBuffer = nullptr;
  mSharedData = new SharedJSAllocatedData(Move(data));
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

template <ActorFlavorEnum>
struct BlobTraits
{ };

template <>
struct BlobTraits<Parent>
{
  typedef mozilla::dom::BlobParent BlobType;
  typedef mozilla::dom::PBlobParent ProtocolType;
};

template <>
struct BlobTraits<Child>
{
  typedef mozilla::dom::BlobChild BlobType;
  typedef mozilla::dom::PBlobChild ProtocolType;
};

template <ActorFlavorEnum, ManagerFlavorEnum>
struct ParentManagerTraits
{ };

template<>
struct ParentManagerTraits<Parent, ContentProtocol>
{
  typedef mozilla::dom::nsIContentParent ConcreteContentManagerType;
};

template<>
struct ParentManagerTraits<Child, ContentProtocol>
{
  typedef mozilla::dom::nsIContentChild ConcreteContentManagerType;
};

template<>
struct ParentManagerTraits<Parent, BackgroundProtocol>
{
  typedef mozilla::ipc::PBackgroundParent ConcreteContentManagerType;
};

template<>
struct ParentManagerTraits<Child, BackgroundProtocol>
{
  typedef mozilla::ipc::PBackgroundChild ConcreteContentManagerType;
};

template<ActorFlavorEnum>
struct DataBlobs
{ };

template<>
struct DataBlobs<Parent>
{
  typedef BlobTraits<Parent>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }
};

template<>
struct DataBlobs<Child>
{
  typedef BlobTraits<Child>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }
};

template<ActorFlavorEnum Flavor, ManagerFlavorEnum ManagerFlavor>
static bool
BuildClonedMessageData(typename ParentManagerTraits<Flavor, ManagerFlavor>::ConcreteContentManagerType* aManager,
                       StructuredCloneData& aData,
                       ClonedMessageData& aClonedData)
{
  SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  auto iter = aData.Data().Iter();
  size_t size = aData.Data().Size();
  bool success;
  buffer.data = aData.Data().Borrow<js::SystemAllocPolicy>(iter, size, &success);
  if (NS_WARN_IF(!success)) {
    return false;
  }
  if (aData.SupportsTransferring()) {
    aClonedData.identfiers().AppendElements(aData.PortIdentifiers());
  }

  const nsTArray<RefPtr<BlobImpl>>& blobImpls = aData.BlobImpls();

  if (!blobImpls.IsEmpty()) {
    typedef typename BlobTraits<Flavor>::BlobType BlobType;
    typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
    InfallibleTArray<ProtocolType*>& blobList = DataBlobs<Flavor>::Blobs(aClonedData);
    uint32_t length = blobImpls.Length();
    blobList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      BlobType* protocolActor = BlobType::GetOrCreate(aManager, blobImpls[i]);
      if (!protocolActor) {
        return false;
      }
      blobList.AppendElement(protocolActor);
    }
  }
  return true;
}

bool
StructuredCloneData::BuildClonedMessageDataForParent(
  nsIContentParent* aParent,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Parent, ContentProtocol>(aParent, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForChild(
  nsIContentChild* aChild,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Child, ContentProtocol>(aChild, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForBackgroundParent(
  PBackgroundParent* aParent,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Parent, BackgroundProtocol>(aParent, *this, aClonedData);
}

bool
StructuredCloneData::BuildClonedMessageDataForBackgroundChild(
  PBackgroundChild* aChild,
  ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Child, BackgroundProtocol>(aChild, *this, aClonedData);
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
  typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
  const InfallibleTArray<ProtocolType*>& blobs = DataBlobs<Flavor>::Blobs(aClonedData);
  const InfallibleTArray<MessagePortIdentifier>& identifiers = aClonedData.identfiers();

  MemoryTraits<MemoryFlavor>::ProvideBuffer(aClonedData, aData);

  if (aData.SupportsTransferring()) {
    aData.PortIdentifiers().AppendElements(identifiers);
  }

  if (!blobs.IsEmpty()) {
    uint32_t length = blobs.Length();
    aData.BlobImpls().SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      auto* blob =
        static_cast<typename BlobTraits<Flavor>::BlobType*>(blobs[i]);
      MOZ_ASSERT(blob);

      RefPtr<BlobImpl> blobImpl = blob->GetBlobImpl();
      MOZ_ASSERT(blobImpl);

      aData.BlobImpls().AppendElement(blobImpl);
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
  UnpackClonedMessageData<BorrowMemory, Parent>(aClonedData, *this);
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
  JSStructuredCloneData data;
  if (!ReadParam(aMsg, aIter, &data)) {
    return false;
  }
  mSharedData = new SharedJSAllocatedData(Move(data));
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
  mSharedData = new SharedJSAllocatedData(Move(aData));
  mInitialized = true;
  return true;
}

} // namespace ipc
} // namespace dom
} // namespace mozilla
