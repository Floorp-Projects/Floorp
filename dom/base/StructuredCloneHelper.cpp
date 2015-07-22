/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneHelper.h"

#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/StructuredCloneTags.h"

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
  NS_WARNING("Failed to clone data for the Console API in workers.");
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

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);
  return mBuffer->write(aCx, aValue, &gCallbacks, this);
}

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     JS::Handle<JS::Value> aTransfer)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);
  return mBuffer->write(aCx, aValue, aTransfer, &gCallbacks, this);
}

bool
StructuredCloneHelperInternal::Read(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mBuffer, "Read() without Write() is not allowed.");

  bool ok = mBuffer->read(aCx, aValue, &gCallbacks, this);
  mBuffer = nullptr;
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

StructuredCloneHelper::StructuredCloneHelper(uint32_t aFlags)
  : mFlags(aFlags)
  , mParent(nullptr)
{}

StructuredCloneHelper::~StructuredCloneHelper()
{}

bool
StructuredCloneHelper::Write(JSContext* aCx,
                             JS::Handle<JS::Value> aValue,
                             JS::Handle<JS::Value> aTransfer)
{
  bool ok = StructuredCloneHelperInternal::Write(aCx, aValue, aTransfer);
  mTransferringPort.Clear();
  return ok;
}

bool
StructuredCloneHelper::Read(nsISupports* aParent,
                            JSContext* aCx,
                            JS::MutableHandle<JS::Value> aValue)
{
  mozilla::AutoRestore<nsISupports*> guard(mParent);
  mParent = aParent;

  return StructuredCloneHelperInternal::Read(aCx, aValue);
}

JSObject*
StructuredCloneHelper::ReadCallback(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    uint32_t aTag,
                                    uint32_t aIndex)
{
  if (aTag == SCTAG_DOM_BLOB) {
    MOZ_ASSERT(!(mFlags & eBlobNotSupported));

    BlobImpl* blobImpl;
    if (JS_ReadBytes(aReader, &blobImpl, sizeof(blobImpl))) {
      MOZ_ASSERT(blobImpl);

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
  }

  if (aTag == SCTAG_DOM_FILELIST) {
    MOZ_ASSERT(!(mFlags & eFileListNotSupported));

    FileListClonedData* fileListClonedData;
    if (JS_ReadBytes(aReader, &fileListClonedData,
                     sizeof(fileListClonedData))) {
      MOZ_ASSERT(fileListClonedData);

      // nsRefPtr<FileList> needs to go out of scope before toObjectOrNull() is
      // called because the static analysis thinks dereferencing XPCOM objects
      // can GC (because in some cases it can!), and a return statement with a
      // JSObject* type means that JSObject* is on the stack as a raw pointer
      // while destructors are running.
      JS::Rooted<JS::Value> val(aCx);
      {
        nsRefPtr<FileList> fileList =
          FileList::Create(mParent, fileListClonedData);
        if (!fileList || !ToJSValue(aCx, fileList, &val)) {
          return nullptr;
        }
      }

      return &val.toObject();
    }
  }

  return NS_DOMReadStructuredClone(aCx, aReader, aTag, aIndex, nullptr);
}

bool
StructuredCloneHelper::WriteCallback(JSContext* aCx,
                                     JSStructuredCloneWriter* aWriter,
                                     JS::Handle<JSObject*> aObj)
{
  // See if this is a File/Blob object.
  if (!(mFlags & eBlobNotSupported)) {
    Blob* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob))) {
      BlobImpl* blobImpl = blob->Impl();
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB, 0) &&
             JS_WriteBytes(aWriter, &blobImpl, sizeof(blobImpl)) &&
             StoreISupports(blobImpl);
    }
  }

  if (!(mFlags & eFileListNotSupported)) {
    FileList* fileList = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FileList, aObj, fileList))) {
      nsRefPtr<FileListClonedData> fileListClonedData =
        fileList->CreateClonedData();
      MOZ_ASSERT(fileListClonedData);
      FileListClonedData* ptr = fileListClonedData.get();
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_FILELIST, 0) &&
             JS_WriteBytes(aWriter, &ptr, sizeof(ptr)) &&
             StoreISupports(fileListClonedData);
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
  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!(mFlags & eMessagePortNotSupported));

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
  if (!(mFlags & eMessagePortNotSupported)) {
    MessagePortBase* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
    if (NS_SUCCEEDED(rv)) {
      if (mTransferringPort.Contains(port)) {
        // No duplicates.
        return false;
      }

      // We use aExtraData to store the index of this new port identifier.
      *aExtraData = mPortIdentifiers.Length();
      MessagePortIdentifier* identifier = mPortIdentifiers.AppendElement();

      if (!port->CloneAndDisentangle(*identifier)) {
        return false;
      }

      mTransferringPort.AppendElement(port);

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
  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!(mFlags & eMessagePortNotSupported));
    MOZ_ASSERT(!aContent);
    MOZ_ASSERT(aExtraData < mPortIdentifiers.Length());
    MessagePort::ForceClose(mPortIdentifiers[aExtraData]);
  }
}

} // dom namespace
} // mozilla namespace
