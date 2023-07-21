/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneHolder_h
#define mozilla_dom_StructuredCloneHolder_h

#include <cstddef>
#include <cstdint>
#include <utility>
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsIGlobalObject;
class nsIInputStream;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace JS {
class Value;
struct WasmModule;
}  // namespace JS

namespace mozilla {
class ErrorResult;
template <class T>
class OwningNonNull;

namespace layers {
class Image;
}

namespace gfx {
class DataSourceSurface;
}

namespace dom {

class BlobImpl;
class MessagePort;
class MessagePortIdentifier;
template <typename T>
class Sequence;

class StructuredCloneHolderBase {
 public:
  typedef JS::StructuredCloneScope StructuredCloneScope;

  StructuredCloneHolderBase(
      StructuredCloneScope aScope = StructuredCloneScope::SameProcess);
  virtual ~StructuredCloneHolderBase();

  // Note, it is unsafe to std::move() a StructuredCloneHolderBase since a raw
  // this pointer is passed to mBuffer as a callback closure.  That must
  // be fixed if you want to implement a move constructor here.
  StructuredCloneHolderBase(StructuredCloneHolderBase&& aOther) = delete;

  // These methods should be implemented in order to clone data.
  // Read more documentation in js/public/StructuredClone.h.

  virtual JSObject* CustomReadHandler(
      JSContext* aCx, JSStructuredCloneReader* aReader,
      const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
      uint32_t aIndex) = 0;

  virtual bool CustomWriteHandler(JSContext* aCx,
                                  JSStructuredCloneWriter* aWriter,
                                  JS::Handle<JSObject*> aObj,
                                  bool* aSameProcessScopeRequired) = 0;

  // This method has to be called when this object is not needed anymore.
  // It will free memory and the buffer. This has to be called because
  // otherwise the buffer will be freed in the DTOR of this class and at that
  // point we cannot use the overridden methods.
  void Clear();

  // If these 3 methods are not implement, transfering objects will not be
  // allowed. Otherwise only arrayBuffers will be transferred.

  virtual bool CustomReadTransferHandler(
      JSContext* aCx, JSStructuredCloneReader* aReader,
      const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
      void* aContent, uint64_t aExtraData,
      JS::MutableHandle<JSObject*> aReturnObject);

  virtual bool CustomWriteTransferHandler(JSContext* aCx,
                                          JS::Handle<JSObject*> aObj,
                                          // Output:
                                          uint32_t* aTag,
                                          JS::TransferableOwnership* aOwnership,
                                          void** aContent,
                                          uint64_t* aExtraData);

  virtual void CustomFreeTransferHandler(uint32_t aTag,
                                         JS::TransferableOwnership aOwnership,
                                         void* aContent, uint64_t aExtraData);

  virtual bool CustomCanTransferHandler(JSContext* aCx,
                                        JS::Handle<JSObject*> aObj,
                                        bool* aSameProcessScopeRequired);

  // These methods are what you should use to read/write data.

  // Execute the serialization of aValue using the Structured Clone Algorithm.
  // The data can read back using Read().
  bool Write(JSContext* aCx, JS::Handle<JS::Value> aValue);

  // Like Write() but it supports the transferring of objects and handling
  // of cloning policy.
  bool Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer,
             const JS::CloneDataPolicy& aCloneDataPolicy);

  // If Write() has been called, this method retrieves data and stores it into
  // aValue.
  bool Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

  // Like Read() but it supports handling of clone policy.
  bool Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
            const JS::CloneDataPolicy& aCloneDataPolicy);

  bool HasData() const { return !!mBuffer; }

  JSStructuredCloneData& BufferData() const {
    MOZ_ASSERT(mBuffer, "Write() has never been called.");
    return mBuffer->data();
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
    size_t size = 0;
    if (HasData()) {
      size += mBuffer->sizeOfIncludingThis(aMallocSizeOf);
    }
    return size;
  }

  void SetErrorMessage(const char* aErrorMessage) {
    mErrorMessage.Assign(aErrorMessage);
  }

 protected:
  UniquePtr<JSAutoStructuredCloneBuffer> mBuffer;

  StructuredCloneScope mStructuredCloneScope;

  // Error message when a data clone error is about to throw. It's held while
  // the error callback is fired and it will be throw with a data clone error
  // later.
  nsCString mErrorMessage;

#ifdef DEBUG
  bool mClearCalled;
#endif
};

class BlobImpl;
class EncodedVideoChunkData;
class MessagePort;
class MessagePortIdentifier;
struct VideoFrameSerializedData;

class StructuredCloneHolder : public StructuredCloneHolderBase {
 public:
  enum CloningSupport { CloningSupported, CloningNotSupported };

  enum TransferringSupport { TransferringSupported, TransferringNotSupported };

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

  StructuredCloneHolder(StructuredCloneHolder&& aOther) = delete;

  // Normally you should just use Write() and Read().

  virtual void Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     ErrorResult& aRv);

  virtual void Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     JS::Handle<JS::Value> aTransfer,
                     const JS::CloneDataPolicy& aCloneDataPolicy,
                     ErrorResult& aRv);

  void Read(nsIGlobalObject* aGlobal, JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue, ErrorResult& aRv);

  void Read(nsIGlobalObject* aGlobal, JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            const JS::CloneDataPolicy& aCloneDataPolicy, ErrorResult& aRv);

  // Call this method to know if this object is keeping some DOM object alive.
  bool HasClonedDOMObjects() const {
    return !mBlobImplArray.IsEmpty() || !mWasmModuleArray.IsEmpty() ||
           !mClonedSurfaces.IsEmpty() || !mInputStreamArray.IsEmpty() ||
           !mVideoFrames.IsEmpty() || !mEncodedVideoChunks.IsEmpty();
  }

  nsTArray<RefPtr<BlobImpl>>& BlobImpls() {
    MOZ_ASSERT(mSupportsCloning,
               "Blobs cannot be taken/set if cloning is not supported.");
    return mBlobImplArray;
  }

  nsTArray<RefPtr<JS::WasmModule>>& WasmModules() {
    MOZ_ASSERT(mSupportsCloning,
               "WasmModules cannot be taken/set if cloning is not supported.");
    return mWasmModuleArray;
  }

  nsTArray<nsCOMPtr<nsIInputStream>>& InputStreams() {
    MOZ_ASSERT(mSupportsCloning,
               "InputStreams cannot be taken/set if cloning is not supported.");
    return mInputStreamArray;
  }

  // This method returns the final scope. If the final scope is unknown,
  // DifferentProcess is returned because it's the most restrictive one.
  StructuredCloneScope CloneScope() const {
    if (mStructuredCloneScope == StructuredCloneScope::UnknownDestination) {
      return StructuredCloneScope::DifferentProcess;
    }
    return mStructuredCloneScope;
  }

  // The global object is set internally just during the Read(). This method
  // can be used by read functions to retrieve it.
  nsIGlobalObject* GlobalDuringRead() const { return mGlobal; }

  // This must be called if the transferring has ports generated by Read().
  // MessagePorts are not thread-safe and they must be retrieved in the thread
  // where they are created.
  nsTArray<RefPtr<MessagePort>>&& TakeTransferredPorts() {
    MOZ_ASSERT(mSupportsTransferring);
    return std::move(mTransferredPorts);
  }

  // This method uses TakeTransferredPorts() to populate a sequence of
  // MessagePorts for WebIDL binding classes.
  bool TakeTransferredPortsAsSequence(
      Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts);

  nsTArray<MessagePortIdentifier>& PortIdentifiers() const {
    MOZ_ASSERT(mSupportsTransferring);
    return mPortIdentifiers;
  }

  nsTArray<RefPtr<gfx::DataSourceSurface>>& GetSurfaces() {
    return mClonedSurfaces;
  }

  nsTArray<VideoFrameSerializedData>& VideoFrames() { return mVideoFrames; }

  nsTArray<EncodedVideoChunkData>& EncodedVideoChunks() {
    return mEncodedVideoChunks;
  }

  // Implementations of the virtual methods to allow cloning of objects which
  // JS engine itself doesn't clone.

  virtual JSObject* CustomReadHandler(
      JSContext* aCx, JSStructuredCloneReader* aReader,
      const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
      uint32_t aIndex) override;

  virtual bool CustomWriteHandler(JSContext* aCx,
                                  JSStructuredCloneWriter* aWriter,
                                  JS::Handle<JSObject*> aObj,
                                  bool* aSameProcessScopeRequired) override;

  virtual bool CustomReadTransferHandler(
      JSContext* aCx, JSStructuredCloneReader* aReader,
      const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
      void* aContent, uint64_t aExtraData,
      JS::MutableHandle<JSObject*> aReturnObject) override;

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

  virtual bool CustomCanTransferHandler(
      JSContext* aCx, JS::Handle<JSObject*> aObj,
      bool* aSameProcessScopeRequired) override;

  // These 2 static methods are useful to read/write fully serializable objects.
  // They can be used by custom StructuredCloneHolderBase classes to
  // serialize objects such as ImageData, CryptoKey, RTCCertificate, etc.

  static JSObject* ReadFullySerializableObjects(
      JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
      bool aIsForIndexedDB);

  static bool WriteFullySerializableObjects(JSContext* aCx,
                                            JSStructuredCloneWriter* aWriter,
                                            JS::Handle<JSObject*> aObj);

  // Helper functions for reading and writing strings.
  static bool ReadString(JSStructuredCloneReader* aReader, nsString& aString);
  static bool WriteString(JSStructuredCloneWriter* aWriter,
                          const nsAString& aString);
  static bool ReadCString(JSStructuredCloneReader* aReader, nsCString& aString);
  static bool WriteCString(JSStructuredCloneWriter* aWriter,
                           const nsACString& aString);

  static const JSStructuredCloneCallbacks sCallbacks;

 protected:
  // If you receive a buffer from IPC, you can use this method to retrieve a
  // JS::Value. It can happen that you want to pre-populate the array of Blobs
  // and/or the PortIdentifiers.
  void ReadFromBuffer(nsIGlobalObject* aGlobal, JSContext* aCx,
                      JSStructuredCloneData& aBuffer,
                      JS::MutableHandle<JS::Value> aValue,
                      const JS::CloneDataPolicy& aCloneDataPolicy,
                      ErrorResult& aRv);

  void ReadFromBuffer(nsIGlobalObject* aGlobal, JSContext* aCx,
                      JSStructuredCloneData& aBuffer,
                      uint32_t aAlgorithmVersion,
                      JS::MutableHandle<JS::Value> aValue,
                      const JS::CloneDataPolicy& aCloneDataPolicy,
                      ErrorResult& aRv);

  void SameProcessScopeRequired(bool* aSameProcessScopeRequired);

  already_AddRefed<MessagePort> ReceiveMessagePort(uint64_t aIndex);

  bool mSupportsCloning;
  bool mSupportsTransferring;

  // SizeOfExcludingThis is inherited from StructuredCloneHolderBase. It doesn't
  // account for objects in the following arrays because a) they're not expected
  // to be stored in long-lived StructuredCloneHolder objects, and b) in the
  // case of BlobImpl objects, MemoryBlobImpls have their own memory reporters,
  // and the other types do not hold significant amounts of memory alive.

  // Used for cloning blobs in the structured cloning algorithm.
  nsTArray<RefPtr<BlobImpl>> mBlobImplArray;

  // Used for cloning JS::WasmModules in the structured cloning algorithm.
  nsTArray<RefPtr<JS::WasmModule>> mWasmModuleArray;

  // Used for cloning InputStream in the structured cloning algorithm.
  nsTArray<nsCOMPtr<nsIInputStream>> mInputStreamArray;

  // This is used for sharing the backend of ImageBitmaps.
  // The DataSourceSurface object must be thread-safely reference-counted.
  // The DataSourceSurface object will not be written ever via any ImageBitmap
  // instance, so no race condition will occur.
  nsTArray<RefPtr<gfx::DataSourceSurface>> mClonedSurfaces;

  // Used for cloning VideoFrame in the structured cloning algorithm.
  nsTArray<VideoFrameSerializedData> mVideoFrames;

  // Used for cloning EncodedVideoChunk in the structured cloning algorithm.
  nsTArray<EncodedVideoChunkData> mEncodedVideoChunks;

  // This raw pointer is only set within ::Read() and is unset by the end.
  nsIGlobalObject* MOZ_NON_OWNING_REF mGlobal;

  // This array contains the ports once we've finished the reading. It's
  // generated from the mPortIdentifiers array.
  nsTArray<RefPtr<MessagePort>> mTransferredPorts;

  // This array contains the identifiers of the MessagePorts. Based on these we
  // are able to reconnect the new transferred ports with the other
  // MessageChannel ports.
  mutable nsTArray<MessagePortIdentifier> mPortIdentifiers;

#ifdef DEBUG
  nsCOMPtr<nsIEventTarget> mCreationEventTarget;
#endif
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_StructuredCloneHolder_h
