/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneHolder_h
#define mozilla_dom_StructuredCloneHolder_h

#include "js/StructuredClone.h"
#include "mozilla/Move.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

#ifdef DEBUG
#include "nsIThread.h"
#endif

namespace mozilla {
class ErrorResult;
namespace layers {
class Image;
}

namespace gfx {
class DataSourceSurface;
}

namespace dom {

class StructuredCloneHolderBase
{
public:
  typedef JS::StructuredCloneScope StructuredCloneScope;

  StructuredCloneHolderBase(StructuredCloneScope aScope = StructuredCloneScope::SameProcessSameThread);
  virtual ~StructuredCloneHolderBase();

  // These methods should be implemented in order to clone data.
  // Read more documentation in js/public/StructuredClone.h.

  virtual JSObject* CustomReadHandler(JSContext* aCx,
                                      JSStructuredCloneReader* aReader,
                                      uint32_t aTag,
                                      uint32_t aIndex) = 0;

  virtual bool CustomWriteHandler(JSContext* aCx,
                                  JSStructuredCloneWriter* aWriter,
                                  JS::Handle<JSObject*> aObj) = 0;

  // This method has to be called when this object is not needed anymore.
  // It will free memory and the buffer. This has to be called because
  // otherwise the buffer will be freed in the DTOR of this class and at that
  // point we cannot use the overridden methods.
  void Clear();

  // If these 3 methods are not implement, transfering objects will not be
  // allowed. Otherwise only arrayBuffers will be transferred.

  virtual bool
  CustomReadTransferHandler(JSContext* aCx,
                            JSStructuredCloneReader* aReader,
                            uint32_t aTag,
                            void* aContent,
                            uint64_t aExtraData,
                            JS::MutableHandleObject aReturnObject);

  virtual bool
  CustomWriteTransferHandler(JSContext* aCx,
                             JS::Handle<JSObject*> aObj,
                             // Output:
                             uint32_t* aTag,
                             JS::TransferableOwnership* aOwnership,
                             void** aContent,
                             uint64_t* aExtraData);

  virtual void
  CustomFreeTransferHandler(uint32_t aTag,
                            JS::TransferableOwnership aOwnership,
                            void* aContent,
                            uint64_t aExtraData);

  // These methods are what you should use to read/write data.

  // Execute the serialization of aValue using the Structured Clone Algorithm.
  // The data can read back using Read().
  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue);

  // Like Write() but it supports the transferring of objects.
  bool Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer);

  // If Write() has been called, this method retrieves data and stores it into
  // aValue.
  bool Read(JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue);

  bool HasData() const
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

  StructuredCloneScope mStructuredCloneScope;

#ifdef DEBUG
  bool mClearCalled;
#endif
};

class BlobImpl;
class MessagePort;
class MessagePortIdentifier;

class StructuredCloneHolder : public StructuredCloneHolderBase
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

  // If cloning is supported, this object will clone objects such as Blobs,
  // FileList, ImageData, etc.
  // If transferring is supported, we will transfer MessagePorts and in the
  // future other transferrable objects.
  // The StructuredCloneScope is useful to know where the cloned/transferred
  // data can be read and written. Additional checks about the nature of the
  // objects will be done based on this scope value because not all the
  // objects can be sent between threads or processes.
  explicit StructuredCloneHolder(CloningSupport aSupportsCloning,
                                 TransferringSupport aSupportsTransferring,
                                 StructuredCloneScope aStructuredCloneScope);
  virtual ~StructuredCloneHolder();

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
  // This method 'steals' the internal data from this class.
  // You should free this buffer with StructuredCloneHolder::FreeBuffer().
  void MoveBufferDataToArray(FallibleTArray<uint8_t>& aArray,
                             ErrorResult& aRv);

  // Call this method to know if this object is keeping some DOM object alive.
  bool HasClonedDOMObjects() const
  {
    return !mBlobImplArray.IsEmpty() ||
           !mClonedSurfaces.IsEmpty();
  }

  nsTArray<RefPtr<BlobImpl>>& BlobImpls()
  {
    MOZ_ASSERT(mSupportsCloning, "Blobs cannot be taken/set if cloning is not supported.");
    return mBlobImplArray;
  }

  StructuredCloneScope CloneScope() const
  {
    return mStructuredCloneScope;
  }

  // The parent object is set internally just during the Read(). This method
  // can be used by read functions to retrieve it.
  nsISupports* ParentDuringRead() const
  {
    return mParent;
  }

  // This must be called if the transferring has ports generated by Read().
  // MessagePorts are not thread-safe and they must be retrieved in the thread
  // where they are created.
  nsTArray<RefPtr<MessagePort>>&& TakeTransferredPorts()
  {
    MOZ_ASSERT(mSupportsTransferring);
    return Move(mTransferredPorts);
  }

  nsTArray<MessagePortIdentifier>& PortIdentifiers() const
  {
    MOZ_ASSERT(mSupportsTransferring);
    return mPortIdentifiers;
  }

  nsTArray<RefPtr<gfx::DataSourceSurface>>& GetSurfaces()
  {
    return mClonedSurfaces;
  }

  // Implementations of the virtual methods to allow cloning of objects which
  // JS engine itself doesn't clone.

  virtual JSObject* CustomReadHandler(JSContext* aCx,
                                      JSStructuredCloneReader* aReader,
                                      uint32_t aTag,
                                      uint32_t aIndex) override;

  virtual bool CustomWriteHandler(JSContext* aCx,
                                  JSStructuredCloneWriter* aWriter,
                                  JS::Handle<JSObject*> aObj) override;

  virtual bool CustomReadTransferHandler(JSContext* aCx,
                                         JSStructuredCloneReader* aReader,
                                         uint32_t aTag,
                                         void* aContent,
                                         uint64_t aExtraData,
                                         JS::MutableHandleObject aReturnObject) override;

  virtual bool CustomWriteTransferHandler(JSContext* aCx,
                                          JS::Handle<JSObject*> aObj,
                                          uint32_t* aTag,
                                          JS::TransferableOwnership* aOwnership,
                                          void** aContent,
                                          uint64_t* aExtraData) override;

  virtual void CustomFreeTransferHandler(uint32_t aTag,
                                         JS::TransferableOwnership aOwnership,
                                         void* aContent,
                                         uint64_t aExtraData) override;

  // These 2 static methods are useful to read/write fully serializable objects.
  // They can be used by custom StructuredCloneHolderBase classes to
  // serialize objects such as ImageData, CryptoKey, RTCCertificate, etc.

  static JSObject* ReadFullySerializableObjects(JSContext* aCx,
                                                JSStructuredCloneReader* aReader,
                                                uint32_t aTag);

  static bool  WriteFullySerializableObjects(JSContext* aCx,
                                             JSStructuredCloneWriter* aWriter,
                                             JS::Handle<JSObject*> aObj);

protected:
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

  bool mSupportsCloning;
  bool mSupportsTransferring;

  // Used for cloning blobs in the structured cloning algorithm.
  nsTArray<RefPtr<BlobImpl>> mBlobImplArray;

  // This is used for sharing the backend of ImageBitmaps.
  // The DataSourceSurface object must be thread-safely reference-counted.
  // The DataSourceSurface object will not be written ever via any ImageBitmap
  // instance, so no race condition will occur.
  nsTArray<RefPtr<gfx::DataSourceSurface>> mClonedSurfaces;

  // This raw pointer is only set within ::Read() and is unset by the end.
  nsISupports* MOZ_NON_OWNING_REF mParent;

  // This array contains the ports once we've finished the reading. It's
  // generated from the mPortIdentifiers array.
  nsTArray<RefPtr<MessagePort>> mTransferredPorts;

  // This array contains the identifiers of the MessagePorts. Based on these we
  // are able to reconnect the new transferred ports with the other
  // MessageChannel ports.
  mutable nsTArray<MessagePortIdentifier> mPortIdentifiers;

#ifdef DEBUG
  nsCOMPtr<nsIThread> mCreationThread;
#endif
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_StructuredCloneHolder_h
