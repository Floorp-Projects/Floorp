/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_StructuredCloneData_h
#define mozilla_dom_ipc_StructuredCloneData_h

#include <algorithm>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsISupportsImpl.h"
#include "nsIInputStream.h"

namespace IPC {
class Message;
}
class PickleIterator;

namespace mozilla {
namespace ipc {

class AutoIPCStream;
class PBackgroundChild;
class PBackgroundParent;

} // namespace ipc

namespace dom {

class nsIContentChild;
class nsIContentParent;

namespace ipc {

/**
 * Wraps the non-reference-counted JSStructuredCloneData class to have a
 * reference count so that multiple StructuredCloneData instances can reference
 * a single underlying serialized representation.
 *
 * As used by StructuredCloneData, it is an invariant that our
 * JSStructuredCloneData owns its buffers.  (For the non-owning case,
 * StructuredCloneData uses mExternalData which holds a BufferList::Borrow()ed
 * read-only view of the data.)
 */
class SharedJSAllocatedData final
{
public:
  explicit SharedJSAllocatedData(JSStructuredCloneData&& aData)
    : mData(Move(aData))
  { }

  static already_AddRefed<SharedJSAllocatedData>
  CreateFromExternalData(const char* aData, size_t aDataLength)
  {
    JSStructuredCloneData buf(JS::StructuredCloneScope::DifferentProcess);
    buf.AppendBytes(aData, aDataLength);
    RefPtr<SharedJSAllocatedData> sharedData =
      new SharedJSAllocatedData(Move(buf));
    return sharedData.forget();
  }

  static already_AddRefed<SharedJSAllocatedData>
  CreateFromExternalData(const JSStructuredCloneData& aData)
  {
    JSStructuredCloneData buf(aData.scope());
    buf.Append(aData);
    RefPtr<SharedJSAllocatedData> sharedData =
      new SharedJSAllocatedData(Move(buf));
    return sharedData.forget();
  }

  NS_INLINE_DECL_REFCOUNTING(SharedJSAllocatedData)

  JSStructuredCloneData& Data() { return mData; }
  size_t DataLength() const { return mData.Size(); }

private:
  ~SharedJSAllocatedData() { }

  JSStructuredCloneData mData;
};

/**
 * IPC-aware StructuredCloneHolder subclass that serves as both a helper class
 * for dealing with message data (blobs, transferables) and also an IPDL
 * data-type in cases where message data is not needed.  If your use-case does
 * not (potentially) involve IPC, then you should use StructuredCloneHolder or
 * one of its other subclasses instead.
 *
 * ## Usage ##
 *
 * The general recipe for using this class is:
 * - In your IPDL definition, use the ClonedMessageData type whenever you want
 *   to send a structured clone that may include blobs or transferables such as
 *   message ports.
 * - To send the data, instantiate a StructuredCloneData instance and Write()
 *   into it like a normal structure clone.  When you are ready to send the
 *   ClonedMessageData-bearing IPC message, use the appropriate
 *   BuildClonedMessageDataFor{Parent,Child,BackgroundParent,BackgroundChild}
 *   method to populate the ClonedMessageData and then send it before your
 *   StructuredCloneData instance is destroyed.  (Buffer borrowing is used
 *   under-the-hood to avoid duplicating the serialized data, requiring this.)
 * - To receive the data, instantiate a StructuredCloneData and use the
 *   appropriate {Borrow,Copy,Steal}FromClonedMessageDataFor{Parent,Child,
 *   BackgroundParent,BackgroundChild} method.  See the memory management
 *   section for more info.
 *
 * Variations:
 * - If transferables are not allowed (ex: BroadcastChannel), then use the
 *   StructuredCloneDataNoTransfers subclass instead of StructuredCloneData.
 *
 * ## Memory Management ##
 *
 * Serialized structured clone representations can be quite large.  So it's best
 * to avoid wasteful duplication.  When Write()ing into the StructuredCloneData,
 * you don't need to worry about this[1], but when you already have serialized
 * structured clone data you plan to Read(), you do need to.  Similarly, if
 * you're using StructuredCloneData as an IPDL type, it efficiently unmarshals.
 *
 * The from-ClonedMessageData memory management strategies available are:
 * - Borrow: Create a JSStructuredCloneData that holds a non-owning, read-only
 *   BufferList::Borrow()ed copy of the source.  Your StructuredCloneData needs
 *   to be destroyed before the source is.  Commonly used when the
 *   StructuredCloneData instance is stack-allocated (and Read() is used before
 *   the function returns).
 * - Copy: Makes a reference-counted copy of the source JSStructuredCloneData,
 *   making it safe for the StructuredCloneData to outlive the source data.
 * - Steal: Steal the buffers from the underlying JSStructuredCloneData so that
 *   it's safe for the StructuredCloneData to outlive the source data.  This is
 *   safe to use with IPC-provided ClonedMessageData instances because
 *   JSStructuredCloneData's IPC ParamTraits::Read method uses ExtractBuffers,
 *   returning a fatal false if unable to extract.  (And
 *   SerializedStructuredCloneBuffer wraps/defers to it.)  But if it's possible
 *   the ClonedMessageData came from a different source that might have borrowed
 *   the buffers itself, then things will crash.  That would be a pretty crazy
 *   implementation; if you see one, change it to use SharedJSAllocatedData.
 *
 * 1: Specifically, in the Write() case an owning SharedJSAllocatedData is
 *    created efficiently (by stealing from StructuredCloneHolder).  The
 *    BuildClonedMessageDataFor* method can be called at any time and it will
 *    borrow the underlying memory.  While it would be even better if
 *    SerializedStructuredCloneBuffer could hold a SharedJSAllocatedData ref,
 *    there's no reason you can't wait to BuildClonedMessageDataFor* until you
 *    need to make the IPC Send* call.
 */
class StructuredCloneData : public StructuredCloneHolder
{
public:
  StructuredCloneData();

  StructuredCloneData(const StructuredCloneData&) = delete;

  StructuredCloneData(StructuredCloneData&& aOther);

  ~StructuredCloneData();

  StructuredCloneData&
  operator=(const StructuredCloneData& aOther) = delete;

  StructuredCloneData&
  operator=(StructuredCloneData&& aOther);

  const nsTArray<RefPtr<BlobImpl>>& BlobImpls() const
  {
    return mBlobImplArray;
  }

  nsTArray<RefPtr<BlobImpl>>& BlobImpls()
  {
    return mBlobImplArray;
  }

  const nsTArray<nsCOMPtr<nsIInputStream>>& InputStreams() const
  {
    return mInputStreamArray;
  }

  nsTArray<nsCOMPtr<nsIInputStream>>& InputStreams()
  {
    return mInputStreamArray;
  }

  bool Copy(const StructuredCloneData& aData);

  void Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfers,
             ErrorResult &aRv);

  // Actor-varying methods to convert the structured clone stored in this holder
  // by a previous call to Write() into ClonedMessageData IPC representation.
  // (Blobs are represented in IPC by IPCBlob actors, so we need the parent to
  // be able to create them.)
  bool BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                       ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForChild(nsIContentChild* aChild,
                                      ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForBackgroundParent(mozilla::ipc::PBackgroundParent* aParent,
                                                 ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForBackgroundChild(mozilla::ipc::PBackgroundChild* aChild,
                                                ClonedMessageData& aClonedData);

  // Actor-varying and memory-management-strategy-varying methods to initialize
  // this holder from a ClonedMessageData representation.
  void BorrowFromClonedMessageDataForParent(const ClonedMessageData& aClonedData);
  void BorrowFromClonedMessageDataForChild(const ClonedMessageData& aClonedData);
  void BorrowFromClonedMessageDataForBackgroundParent(const ClonedMessageData& aClonedData);
  void BorrowFromClonedMessageDataForBackgroundChild(const ClonedMessageData& aClonedData);

  void CopyFromClonedMessageDataForParent(const ClonedMessageData& aClonedData);
  void CopyFromClonedMessageDataForChild(const ClonedMessageData& aClonedData);
  void CopyFromClonedMessageDataForBackgroundParent(const ClonedMessageData& aClonedData);
  void CopyFromClonedMessageDataForBackgroundChild(const ClonedMessageData& aClonedData);

  // The steal variants of course take a non-const ClonedMessageData.
  void StealFromClonedMessageDataForParent(ClonedMessageData& aClonedData);
  void StealFromClonedMessageDataForChild(ClonedMessageData& aClonedData);
  void StealFromClonedMessageDataForBackgroundParent(ClonedMessageData& aClonedData);
  void StealFromClonedMessageDataForBackgroundChild(ClonedMessageData& aClonedData);


  // Initialize this instance, borrowing the contents of the given
  // JSStructuredCloneData.  You are responsible for ensuring that this
  // StructuredCloneData instance is destroyed before aData is destroyed.
  bool UseExternalData(const JSStructuredCloneData& aData)
  {
    auto iter = aData.Start();
    bool success = false;
    mExternalData = aData.Borrow(iter, aData.Size(), &success);
    mInitialized = true;
    return success;
  }

  // Initialize this instance by copying the given data that probably came from
  // nsStructuredClone doing a base64 decode.  Don't use this.
  bool CopyExternalData(const char* aData, size_t aDataLength);
  // Initialize this instance by copying the contents of an existing
  // JSStructuredCloneData.  Use when this StructuredCloneData instance may
  // outlive aData.
  bool CopyExternalData(const JSStructuredCloneData& aData);

  // Initialize this instance by stealing the contents of aData via Move
  // constructor, clearing the original aData as a side-effect.  This is only
  // safe if aData owns the underlying buffers.  This is the case for instances
  // provided by IPC to Recv calls.
  bool StealExternalData(JSStructuredCloneData& aData);

  JSStructuredCloneData& Data()
  {
    return mSharedData ? mSharedData->Data() : mExternalData;
  }

  const JSStructuredCloneData& Data() const
  {
    return mSharedData ? mSharedData->Data() : mExternalData;
  }

  void InitScope(JS::StructuredCloneScope aScope)
  {
    Data().initScope(aScope);
  }

  size_t DataLength() const
  {
    return mSharedData ? mSharedData->DataLength() : mExternalData.Size();
  }

  SharedJSAllocatedData* SharedData() const
  {
    return mSharedData;
  }

  bool SupportsTransferring()
  {
    return mSupportsTransferring;
  }

  FallibleTArray<mozilla::ipc::AutoIPCStream>& IPCStreams()
  {
    return mIPCStreams;
  }

  // For IPC serialization
  void WriteIPCParams(IPC::Message* aMessage) const;
  bool ReadIPCParams(const IPC::Message* aMessage, PickleIterator* aIter);

protected:
  explicit StructuredCloneData(TransferringSupport aSupportsTransferring);

private:
  JSStructuredCloneData mExternalData;
  RefPtr<SharedJSAllocatedData> mSharedData;

  // This array is needed because AutoIPCStream DTOR must be executed after the
  // sending of the data via IPC. This will be fixed by bug 1353475.
  FallibleTArray<mozilla::ipc::AutoIPCStream> mIPCStreams;
  bool mInitialized;
};

/**
 * For use when transferring should not be supported.
 */
class StructuredCloneDataNoTransfers : public StructuredCloneData
{
public:
  StructuredCloneDataNoTransfers()
    : StructuredCloneData(StructuredCloneHolder::TransferringNotSupported)
  {}
};

} // namespace ipc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_StructuredCloneData_h
