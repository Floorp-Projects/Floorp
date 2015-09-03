/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneHelper_h
#define mozilla_dom_StructuredCloneHelper_h

#include "js/StructuredClone.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

namespace mozilla {
class ErrorResult;
namespace layers {
class Image;
}

namespace dom {

class StructuredCloneHelperInternal
{
public:
  StructuredCloneHelperInternal();
  virtual ~StructuredCloneHelperInternal();

  // These methods should be implemented in order to clone data.
  // Read more documentation in js/public/StructuredClone.h.

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) = 0;

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) = 0;

  // This method has to be called when this object is not needed anymore.
  // It will free memory and the buffer. This has to be called because
  // otherwise the buffer will be freed in the DTOR of this class and at that
  // point we cannot use the overridden methods.
  void Shutdown();

  // If these 3 methods are not implement, transfering objects will not be
  // allowed.

  virtual bool
  ReadTransferCallback(JSContext* aCx,
                       JSStructuredCloneReader* aReader,
                       uint32_t aTag,
                       void* aContent,
                       uint64_t aExtraData,
                       JS::MutableHandleObject aReturnObject);

  virtual bool
  WriteTransferCallback(JSContext* aCx,
                        JS::Handle<JSObject*> aObj,
                        // Output:
                        uint32_t* aTag,
                        JS::TransferableOwnership* aOwnership,
                        void** aContent,
                        uint64_t* aExtraData);

  virtual void
  FreeTransferCallback(uint32_t aTag,
                       JS::TransferableOwnership aOwnership,
                       void* aContent,
                       uint64_t aExtraData);

  // These methods are what you should use.

  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue);

  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer);

  bool Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue);

  bool HasBeenWritten() const
  {
    return !!mBuffer;
  }

  uint64_t* BufferData() const
  {
    MOZ_ASSERT(mBuffer, "Write() has never been called.");
    return mBuffer->data();
  }

  size_t BufferSize() const
  {
    MOZ_ASSERT(mBuffer, "Write() has never been called.");
    return mBuffer->nbytes();
  }

protected:
  nsAutoPtr<JSAutoStructuredCloneBuffer> mBuffer;

#ifdef DEBUG
  bool mShutdownCalled;
#endif
};

class BlobImpl;
class MessagePortBase;
class MessagePortIdentifier;

class StructuredCloneHelper : public StructuredCloneHelperInternal
{
public:
  enum CloningSupport
  {
    CloningSupported,
    CloningNotSupported
  };

  enum TransferringSupport
  {
    TransferringSupported,
    TransferringNotSupported
  };

  enum ContextSupport
  {
    SameProcessSameThread,
    SameProcessDifferentThread,
    DifferentProcess
  };

  // If cloning is supported, this object will clone objects such as Blobs,
  // FileList, ImageData, etc.
  // If transferring is supported, we will transfer MessagePorts and in the
  // future other transferrable objects.
  // The ContextSupport is useful to know where the cloned/transferred data can
  // be read and written. Additional checks about the nature of the objects
  // will be done based on this context value because not all the objects can
  // be sent between threads or processes.
  explicit StructuredCloneHelper(CloningSupport aSupportsCloning,
                                 TransferringSupport aSupportsTransferring,
                                 ContextSupport aContextSupport);
  virtual ~StructuredCloneHelper();

  // Normally you should just use Write() and Read().

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             ErrorResult &aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer,
             ErrorResult &aRv);

  void Read(nsISupports* aParent,
            JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            ErrorResult &aRv);

  // Sometimes, when IPC is involved, you must send a buffer after a Write().
  // This method 'steals' the internal data from this helper class.
  // You should free this buffer with FreeBuffer().
  void MoveBufferDataToArray(FallibleTArray<uint8_t>& aArray,
                             ErrorResult& aRv);

  // If you receive a buffer from IPC, you can use this method to retrieve a
  // JS::Value. It can happen that you want to pre-populate the array of Blobs
  // and/or the PortIdentifiers.
  void ReadFromBuffer(nsISupports* aParent,
                      JSContext* aCx,
                      uint64_t* aBuffer,
                      size_t aBufferLength,
                      JS::MutableHandle<JS::Value> aValue,
                      ErrorResult &aRv);

  void ReadFromBuffer(nsISupports* aParent,
                      JSContext* aCx,
                      uint64_t* aBuffer,
                      size_t aBufferLength,
                      uint32_t aAlgorithmVersion,
                      JS::MutableHandle<JS::Value> aValue,
                      ErrorResult &aRv);

  // Use this method to free a buffer generated by MoveToBuffer().
  void FreeBuffer(uint64_t* aBuffer,
                  size_t aBufferLength);

  bool HasClonedDOMObjects() const
  {
    return !mBlobImplArray.IsEmpty() ||
           !mClonedImages.IsEmpty();
  }

  nsTArray<nsRefPtr<BlobImpl>>& BlobImpls()
  {
    MOZ_ASSERT(mSupportsCloning, "Blobs cannot be taken/set if cloning is not supported.");
    return mBlobImplArray;
  }

  nsISupports* ParentDuringRead() const
  {
    return mParent;
  }

  // This must be called if the transferring has ports generated by Read().
  // MessagePorts are not thread-safe and they must be retrieved in the thread
  // where they are created.
  void TakeTransferredPorts(nsTArray<nsRefPtr<MessagePortBase>>& aPorts)
  {
    MOZ_ASSERT(mSupportsTransferring);
    MOZ_ASSERT(aPorts.IsEmpty());
    aPorts.SwapElements(mTransferredPorts);
  }

  nsTArray<MessagePortIdentifier>& PortIdentifiers()
  {
    MOZ_ASSERT(mSupportsTransferring);
    return mPortIdentifiers;
  }

  nsTArray<nsRefPtr<layers::Image>>& GetImages()
  {
    return mClonedImages;
  }

  // Custom Callbacks

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) override;

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) override;

  virtual bool ReadTransferCallback(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    uint32_t aTag,
                                    void* aContent,
                                    uint64_t aExtraData,
                                    JS::MutableHandleObject aReturnObject) override;

  virtual bool WriteTransferCallback(JSContext* aCx,
                                     JS::Handle<JSObject*> aObj,
                                     uint32_t* aTag,
                                     JS::TransferableOwnership* aOwnership,
                                     void** aContent,
                                     uint64_t* aExtraData) override;

  virtual void FreeTransferCallback(uint32_t aTag,
                                    JS::TransferableOwnership aOwnership,
                                    void* aContent,
                                    uint64_t aExtraData) override;
protected:
  bool mSupportsCloning;
  bool mSupportsTransferring;
  ContextSupport mContext;

  // Useful for the structured clone algorithm:

  nsTArray<nsRefPtr<BlobImpl>> mBlobImplArray;

  // This is used for sharing the backend of ImageBitmaps.
  // The layers::Image object must be thread-safely reference-counted.
  // The layers::Image object will not be written ever via any ImageBitmap
  // instance, so no race condition will occur.
  nsTArray<nsRefPtr<layers::Image>> mClonedImages;

  // This raw pointer is set and unset into the ::Read(). It's always null
  // outside that method. For this reason it's a raw pointer.
  nsISupports* MOZ_NON_OWNING_REF mParent;

  // This array contains the ports once we've finished the reading. It's
  // generated from the mPortIdentifiers array.
  nsTArray<nsRefPtr<MessagePortBase>> mTransferredPorts;

  // This array contains the identifiers of the MessagePorts. Based on these we
  // are able to reconnect the new transferred ports with the other
  // MessageChannel ports.
  nsTArray<MessagePortIdentifier> mPortIdentifiers;

#ifdef DEBUG
  nsCOMPtr<nsIThread> mCreationThread;
#endif
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_StructuredCloneHelper_h
