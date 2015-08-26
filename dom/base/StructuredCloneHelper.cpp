/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneHelper.h"

#include "ImageContainer.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {
namespace dom {

namespace {

JSObject*
StructuredCloneCallbacksRead(JSContext* aCx,
                             JSStructuredCloneReader* aReader,
                             uint32_t aTag, uint32_t aIndex,
                             void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->ReadCallback(aCx, aReader, aTag, aIndex);
}

bool
StructuredCloneCallbacksWrite(JSContext* aCx,
                              JSStructuredCloneWriter* aWriter,
                              JS::Handle<JSObject*> aObj,
                              void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->WriteCallback(aCx, aWriter, aObj);
}

bool
StructuredCloneCallbacksReadTransfer(JSContext* aCx,
                                     JSStructuredCloneReader* aReader,
                                     uint32_t aTag,
                                     void* aContent,
                                     uint64_t aExtraData,
                                     void* aClosure,
                                     JS::MutableHandleObject aReturnObject)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->ReadTransferCallback(aCx, aReader, aTag, aContent,
                                      aExtraData, aReturnObject);
}

bool
StructuredCloneCallbacksWriteTransfer(JSContext* aCx,
                                      JS::Handle<JSObject*> aObj,
                                      void* aClosure,
                                      // Output:
                                      uint32_t* aTag,
                                      JS::TransferableOwnership* aOwnership,
                                      void** aContent,
                                      uint64_t* aExtraData)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->WriteTransferCallback(aCx, aObj, aTag, aOwnership, aContent,
                                       aExtraData);
}

void
StructuredCloneCallbacksFreeTransfer(uint32_t aTag,
                                     JS::TransferableOwnership aOwnership,
                                     void* aContent,
                                     uint64_t aExtraData,
                                     void* aClosure)
{
  StructuredCloneHelperInternal* helper =
    static_cast<StructuredCloneHelperInternal*>(aClosure);
  MOZ_ASSERT(helper);
  return helper->FreeTransferCallback(aTag, aOwnership, aContent, aExtraData);
}

void
StructuredCloneCallbacksError(JSContext* aCx,
                              uint32_t aErrorId)
{
  NS_WARNING("Failed to clone data.");
}

const JSStructuredCloneCallbacks gCallbacks = {
  StructuredCloneCallbacksRead,
  StructuredCloneCallbacksWrite,
  StructuredCloneCallbacksError,
  StructuredCloneCallbacksReadTransfer,
  StructuredCloneCallbacksWriteTransfer,
  StructuredCloneCallbacksFreeTransfer
};

} // anonymous namespace

// StructuredCloneHelperInternal class

StructuredCloneHelperInternal::StructuredCloneHelperInternal()
#ifdef DEBUG
  : mShutdownCalled(false)
#endif
{}

StructuredCloneHelperInternal::~StructuredCloneHelperInternal()
{
#ifdef DEBUG
  MOZ_ASSERT(mShutdownCalled);
#endif
}

void
StructuredCloneHelperInternal::Shutdown()
{
#ifdef DEBUG
  mShutdownCalled = true;
#endif

  mBuffer = nullptr;
}

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");
  MOZ_ASSERT(!mShutdownCalled, "This method cannot be called after Shutdown.");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);
  return mBuffer->write(aCx, aValue, &gCallbacks, this);
}

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     JS::Handle<JS::Value> aTransfer)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");
  MOZ_ASSERT(!mShutdownCalled, "This method cannot be called after Shutdown.");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);
  return mBuffer->write(aCx, aValue, aTransfer, &gCallbacks, this);
}

bool
StructuredCloneHelperInternal::Read(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mBuffer, "Read() without Write() is not allowed.");
  MOZ_ASSERT(!mShutdownCalled, "This method cannot be called after Shutdown.");

  bool ok = mBuffer->read(aCx, aValue, &gCallbacks, this);
  return ok;
}

bool
StructuredCloneHelperInternal::ReadTransferCallback(JSContext* aCx,
                                                    JSStructuredCloneReader* aReader,
                                                    uint32_t aTag,
                                                    void* aContent,
                                                    uint64_t aExtraData,
                                                    JS::MutableHandleObject aReturnObject)
{
  MOZ_CRASH("Nothing to read.");
  return false;
}

bool
StructuredCloneHelperInternal::WriteTransferCallback(JSContext* aCx,
                                                     JS::Handle<JSObject*> aObj,
                                                     uint32_t* aTag,
                                                     JS::TransferableOwnership* aOwnership,
                                                     void** aContent,
                                                     uint64_t* aExtraData)
{
  // No transfers are supported by default.
  return false;
}

void
StructuredCloneHelperInternal::FreeTransferCallback(uint32_t aTag,
                                                    JS::TransferableOwnership aOwnership,
                                                    void* aContent,
                                                    uint64_t aExtraData)
{
  MOZ_CRASH("Nothing to free.");
}

// StructuredCloneHelper class

StructuredCloneHelper::StructuredCloneHelper(CloningSupport aSupportsCloning,
                                             TransferringSupport aSupportsTransferring)
  : mSupportsCloning(aSupportsCloning == CloningSupported)
  , mSupportsTransferring(aSupportsTransferring == TransferringSupported)
  , mParent(nullptr)
{}

StructuredCloneHelper::~StructuredCloneHelper()
{
  Shutdown();
}

void
StructuredCloneHelper::Write(JSContext* aCx,
                             JS::Handle<JS::Value> aValue,
                             ErrorResult& aRv)
{
  Write(aCx, aValue, JS::UndefinedHandleValue, aRv);
}

void
StructuredCloneHelper::Write(JSContext* aCx,
                             JS::Handle<JS::Value> aValue,
                             JS::Handle<JS::Value> aTransfer,
                             ErrorResult& aRv)
{
  if (!StructuredCloneHelperInternal::Write(aCx, aValue, aTransfer)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
  }
}

void
StructuredCloneHelper::Read(nsISupports* aParent,
                            JSContext* aCx,
                            JS::MutableHandle<JS::Value> aValue,
                            ErrorResult& aRv)
{
  mozilla::AutoRestore<nsISupports*> guard(mParent);
  mParent = aParent;

  if (!StructuredCloneHelperInternal::Read(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
  }

  // If we are tranferring something, we cannot call 'Read()' more than once.
  if (mSupportsTransferring) {
    mBlobImplArray.Clear();
    Shutdown();
  }
}

void
StructuredCloneHelper::ReadFromBuffer(nsISupports* aParent,
                                      JSContext* aCx,
                                      uint64_t* aBuffer,
                                      size_t aBufferLength,
                                      JS::MutableHandle<JS::Value> aValue,
                                      ErrorResult& aRv)
{
  ReadFromBuffer(aParent, aCx, aBuffer, aBufferLength,
                 JS_STRUCTURED_CLONE_VERSION, aValue, aRv);
}

void
StructuredCloneHelper::ReadFromBuffer(nsISupports* aParent,
                                      JSContext* aCx,
                                      uint64_t* aBuffer,
                                      size_t aBufferLength,
                                      uint32_t aAlgorithmVersion,
                                      JS::MutableHandle<JS::Value> aValue,
                                      ErrorResult& aRv)
{
  MOZ_ASSERT(!mBuffer, "ReadFromBuffer() must be called without a Write().");
  MOZ_ASSERT(aBuffer);

  mozilla::AutoRestore<nsISupports*> guard(mParent);
  mParent = aParent;

  if (!JS_ReadStructuredClone(aCx, aBuffer, aBufferLength, aAlgorithmVersion,
                              aValue, &gCallbacks, this)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
  }
}

void
StructuredCloneHelper::MoveBufferDataToArray(FallibleTArray<uint8_t>& aArray,
                                             ErrorResult& aRv)
{
  MOZ_ASSERT(mBuffer, "MoveBuffer() cannot be called without a Write().");

  if (NS_WARN_IF(!aArray.SetLength(BufferSize(), mozilla::fallible))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  uint64_t* buffer;
  size_t size;
  mBuffer->steal(&buffer, &size);
  mBuffer = nullptr;

  memcpy(aArray.Elements(), buffer, size);
  js_free(buffer);
}

void
StructuredCloneHelper::FreeBuffer(uint64_t* aBuffer,
                                  size_t aBufferLength)
{
  MOZ_ASSERT(!mBuffer, "FreeBuffer() must be called without a Write().");
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aBufferLength);

  JS_ClearStructuredClone(aBuffer, aBufferLength, &gCallbacks, this, false);
}

JSObject*
StructuredCloneHelper::ReadCallback(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    uint32_t aTag,
                                    uint32_t aIndex)
{
  MOZ_ASSERT(mSupportsCloning);

  if (aTag == SCTAG_DOM_BLOB) {
    MOZ_ASSERT(aIndex < mBlobImplArray.Length());
    nsRefPtr<BlobImpl> blobImpl =  mBlobImplArray[aIndex];

    // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
    // called because the static analysis thinks dereferencing XPCOM objects
    // can GC (because in some cases it can!), and a return statement with a
    // JSObject* type means that JSObject* is on the stack as a raw pointer
    // while destructors are running.
    JS::Rooted<JS::Value> val(aCx);
    {
      nsRefPtr<Blob> blob = Blob::Create(mParent, blobImpl);
      if (!ToJSValue(aCx, blob, &val)) {
        return nullptr;
      }
    }

    return &val.toObject();
  }

  if (aTag == SCTAG_DOM_FILELIST) {
    JS::Rooted<JS::Value> val(aCx);
    {
      nsRefPtr<FileList> fileList = new FileList(mParent);

      // |aIndex| is the number of BlobImpls to use from |offset|.
      uint32_t tag, offset;
      if (!JS_ReadUint32Pair(aReader, &tag, &offset)) {
        return nullptr;
      }
      MOZ_ASSERT(tag == 0);

      for (uint32_t i = 0; i < aIndex; ++i) {
        uint32_t index = offset + i;
        MOZ_ASSERT(index < mBlobImplArray.Length());

        nsRefPtr<BlobImpl> blobImpl = mBlobImplArray[index];
        MOZ_ASSERT(blobImpl->IsFile());

        nsRefPtr<File> file = File::Create(mParent, blobImpl);
        if (!fileList->Append(file)) {
          return nullptr;
        }
      }

      if (!ToJSValue(aCx, fileList, &val)) {
        return nullptr;
      }
    }

    return &val.toObject();
  }

  if (aTag == SCTAG_DOM_IMAGEBITMAP) {
     // Get the current global object.
     // This can be null.
     nsCOMPtr<nsIGlobalObject> parent = do_QueryInterface(mParent);
     // aIndex is the index of the cloned image.
     return ImageBitmap::ReadStructuredClone(aCx, aReader,
                                             parent, GetImages(), aIndex);
   }

  return NS_DOMReadStructuredClone(aCx, aReader, aTag, aIndex, nullptr);
}

bool
StructuredCloneHelper::WriteCallback(JSContext* aCx,
                                     JSStructuredCloneWriter* aWriter,
                                     JS::Handle<JSObject*> aObj)
{
  if (!mSupportsCloning) {
    return false;
  }

  // See if this is a File/Blob object.
  {
    Blob* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob))) {
      BlobImpl* blobImpl = blob->Impl();
      if (JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                             mBlobImplArray.Length())) {
        mBlobImplArray.AppendElement(blobImpl);
        return true;
      }

      return false;
    }
  }

  {
    FileList* fileList = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FileList, aObj, fileList))) {
      // A FileList is serialized writing the X number of elements and the offset
      // from mBlobImplArray. The Read will take X elements from mBlobImplArray
      // starting from the offset.
      if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_FILELIST,
                              fileList->Length()) ||
          !JS_WriteUint32Pair(aWriter, 0,
                              mBlobImplArray.Length())) {
        return false;
      }

      for (uint32_t i = 0; i < fileList->Length(); ++i) {
        mBlobImplArray.AppendElement(fileList->Item(i)->Impl());
      }

      return true;
    }
  }

  // See if this is an ImageBitmap object.
  {
    ImageBitmap* imageBitmap = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageBitmap, aObj, imageBitmap))) {
      return ImageBitmap::WriteStructuredClone(aWriter,
                                               GetImages(),
                                               imageBitmap);
    }
  }

  return NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr);
}

bool
StructuredCloneHelper::ReadTransferCallback(JSContext* aCx,
                                            JSStructuredCloneReader* aReader,
                                            uint32_t aTag,
                                            void* aContent,
                                            uint64_t aExtraData,
                                            JS::MutableHandleObject aReturnObject)
{
  MOZ_ASSERT(mSupportsTransferring);

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    // This can be null.
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mParent);

    MOZ_ASSERT(aExtraData < mPortIdentifiers.Length());
    const MessagePortIdentifier& portIdentifier = mPortIdentifiers[aExtraData];

    // aExtraData is the index of this port identifier.
    ErrorResult rv;
    nsRefPtr<MessagePort> port =
      MessagePort::Create(window, portIdentifier, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return false;
    }

    mTransferredPorts.AppendElement(port);

    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, port, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    aReturnObject.set(&value.toObject());
    return true;
  }

  return false;
}

bool
StructuredCloneHelper::WriteTransferCallback(JSContext* aCx,
                                             JS::Handle<JSObject*> aObj,
                                             uint32_t* aTag,
                                             JS::TransferableOwnership* aOwnership,
                                             void** aContent,
                                             uint64_t* aExtraData)
{
  if (!mSupportsTransferring) {
    return false;
  }

  {
    MessagePortBase* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
    if (NS_SUCCEEDED(rv)) {
      // We use aExtraData to store the index of this new port identifier.
      *aExtraData = mPortIdentifiers.Length();
      MessagePortIdentifier* identifier = mPortIdentifiers.AppendElement();

      if (!port->CloneAndDisentangle(*identifier)) {
        return false;
      }

      *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
      *aOwnership = JS::SCTAG_TMO_CUSTOM;
      *aContent = nullptr;

      return true;
    }
  }

  return false;
}

void
StructuredCloneHelper::FreeTransferCallback(uint32_t aTag,
                                            JS::TransferableOwnership aOwnership,
                                            void* aContent,
                                            uint64_t aExtraData)
{
  MOZ_ASSERT(mSupportsTransferring);

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!aContent);
    MOZ_ASSERT(aExtraData < mPortIdentifiers.Length());
    MessagePort::ForceClose(mPortIdentifiers[aExtraData]);
  }
}

} // dom namespace
} // mozilla namespace
