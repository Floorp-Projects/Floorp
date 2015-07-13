/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortUtils.h"
#include "MessagePort.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/StructuredCloneTags.h"

namespace mozilla {
namespace dom {
namespace messageport {

namespace {

struct MOZ_STACK_CLASS StructuredCloneClosureInternal
{
  StructuredCloneClosureInternal(
    StructuredCloneClosure& aClosure, nsPIDOMWindow* aWindow)
    : mClosure(aClosure)
    , mWindow(aWindow)
  { }

  StructuredCloneClosure& mClosure;
  nsPIDOMWindow* mWindow;
  nsTArray<nsRefPtr<MessagePort>> mMessagePorts;
  nsTArray<nsRefPtr<MessagePortBase>> mTransferredPorts;
};

struct MOZ_STACK_CLASS StructuredCloneClosureInternalReadOnly
{
  StructuredCloneClosureInternalReadOnly(
    const StructuredCloneClosure& aClosure, nsPIDOMWindow* aWindow)
    : mClosure(aClosure)
    , mWindow(aWindow)
  { }

  const StructuredCloneClosure& mClosure;
  nsPIDOMWindow* mWindow;
  nsTArray<nsRefPtr<MessagePort>> mMessagePorts;
  nsTArray<nsRefPtr<MessagePortBase>> mTransferredPorts;
};

void
Error(JSContext* aCx, uint32_t aErrorId)
{
  if (NS_IsMainThread()) {
    NS_DOMStructuredCloneError(aCx, aErrorId);
  } else {
    Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  }
}

JSObject*
Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
     uint32_t aData, void* aClosure)
{
  MOZ_ASSERT(aClosure);

  auto* closure = static_cast<StructuredCloneClosureInternalReadOnly*>(aClosure);

  if (aTag == SCTAG_DOM_BLOB) {
    // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
    // called because the static analysis thinks dereferencing XPCOM objects
    // can GC (because in some cases it can!), and a return statement with a
    // JSObject* type means that JSObject* is on the stack as a raw pointer
    // while destructors are running.
    JS::Rooted<JS::Value> val(aCx);
    {
      MOZ_ASSERT(aData < closure->mClosure.mBlobImpls.Length());
      nsRefPtr<BlobImpl> blobImpl = closure->mClosure.mBlobImpls[aData];

#ifdef DEBUG
      {
        // Blob should not be mutable.
        bool isMutable;
        MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
        MOZ_ASSERT(!isMutable);
      }
#endif

      // Let's create a new blob with the correct parent.
      nsIGlobalObject* global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
      MOZ_ASSERT(global);

      nsRefPtr<Blob> newBlob = Blob::Create(global, blobImpl);
      if (!ToJSValue(aCx, newBlob, &val)) {
        return nullptr;
      }
    }

    return &val.toObject();
  }

  return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
}

bool
Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
      JS::Handle<JSObject*> aObj, void* aClosure)
{
  MOZ_ASSERT(aClosure);

  auto* closure = static_cast<StructuredCloneClosureInternal*>(aClosure);

  // See if the wrapped native is a File/Blob.
  {
    Blob* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob)) &&
        NS_SUCCEEDED(blob->SetMutable(false)) &&
        JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                           closure->mClosure.mBlobImpls.Length())) {
      closure->mClosure.mBlobImpls.AppendElement(blob->Impl());
      return true;
    }
  }

  return NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr);
}

bool
ReadTransfer(JSContext* aCx, JSStructuredCloneReader* aReader,
             uint32_t aTag, void* aContent, uint64_t aExtraData,
             void* aClosure, JS::MutableHandle<JSObject*> aReturnObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aClosure);

  auto* closure = static_cast<StructuredCloneClosureInternalReadOnly*>(aClosure);

  if (aTag != SCTAG_DOM_MAP_MESSAGEPORT) {
    return false;
  }

  MOZ_ASSERT(aContent == 0);
  MOZ_ASSERT(aExtraData < closure->mClosure.mMessagePortIdentifiers.Length());

  ErrorResult rv;
  nsRefPtr<MessagePort> port =
    MessagePort::Create(closure->mWindow,
                        closure->mClosure.mMessagePortIdentifiers[aExtraData],
                        rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  closure->mMessagePorts.AppendElement(port);

  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, port, &value)) {
    JS_ClearPendingException(aCx);
    return false;
  }

  aReturnObject.set(&value.toObject());
  return true;
}

bool
WriteTransfer(JSContext* aCx, JS::Handle<JSObject*> aObj, void* aClosure,
              uint32_t* aTag, JS::TransferableOwnership* aOwnership,
              void** aContent, uint64_t* aExtraData)
{
  MOZ_ASSERT(aClosure);

  auto* closure = static_cast<StructuredCloneClosureInternal*>(aClosure);

  MessagePortBase* port = nullptr;
  nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (closure->mTransferredPorts.Contains(port)) {
    // No duplicates.
    return false;
  }

  MessagePortIdentifier identifier;
  if (!port->CloneAndDisentangle(identifier)) {
    return false;
  }

  closure->mClosure.mMessagePortIdentifiers.AppendElement(identifier);
  closure->mTransferredPorts.AppendElement(port);

  *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
  *aOwnership = JS::SCTAG_TMO_CUSTOM;
  *aContent = nullptr;
  *aExtraData = closure->mClosure.mMessagePortIdentifiers.Length() - 1;

  return true;
}

void
FreeTransfer(uint32_t aTag, JS::TransferableOwnership aOwnership,
             void* aContent, uint64_t aExtraData, void* aClosure)
{
  MOZ_ASSERT(aClosure);
  auto* closure = static_cast<StructuredCloneClosureInternal*>(aClosure);

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!aContent);
    MOZ_ASSERT(aExtraData < closure->mClosure.mMessagePortIdentifiers.Length());
    MessagePort::ForceClose(closure->mClosure.mMessagePortIdentifiers[(uint32_t)aExtraData]);
  }
}

const JSStructuredCloneCallbacks gCallbacks = {
  Read,
  Write,
  Error,
  ReadTransfer,
  WriteTransfer,
  FreeTransfer,
};

} // namespace

bool
ReadStructuredCloneWithTransfer(JSContext* aCx, nsTArray<uint8_t>& aData,
                                const StructuredCloneClosure& aClosure,
                                JS::MutableHandle<JS::Value> aClone,
                                nsPIDOMWindow* aParentWindow,
                                nsTArray<nsRefPtr<MessagePort>>& aMessagePorts)
{
  auto* data = reinterpret_cast<uint64_t*>(aData.Elements());
  size_t dataLen = aData.Length();
  MOZ_ASSERT(!(dataLen % sizeof(*data)));

  StructuredCloneClosureInternalReadOnly internalClosure(aClosure,
                                                         aParentWindow);

  bool rv = JS_ReadStructuredClone(aCx, data, dataLen,
                                   JS_STRUCTURED_CLONE_VERSION, aClone,
                                   &gCallbacks, &internalClosure);
  if (rv) {
    aMessagePorts.SwapElements(internalClosure.mMessagePorts);
  }

  return rv;
}

bool
WriteStructuredCloneWithTransfer(JSContext* aCx, JS::Handle<JS::Value> aSource,
                                 JS::Handle<JS::Value> aTransferable,
                                 nsTArray<uint8_t>& aData,
                                 StructuredCloneClosure& aClosure)
{
  StructuredCloneClosureInternal internalClosure(aClosure, nullptr);
  JSAutoStructuredCloneBuffer buffer(&gCallbacks, &internalClosure);

  if (!buffer.write(aCx, aSource, aTransferable, &gCallbacks,
                    &internalClosure)) {
    return false;
  }

  FallibleTArray<uint8_t> cloneData;
  if (NS_WARN_IF(!cloneData.SetLength(buffer.nbytes(), mozilla::fallible))) {
    return false;
  }

  uint64_t* data;
  size_t size;
  buffer.steal(&data, &size);

  memcpy(cloneData.Elements(), data, size);
  js_free(data);

  MOZ_ASSERT(aData.IsEmpty());
  aData.SwapElements(cloneData);
  return true;
}

void
FreeStructuredClone(nsTArray<uint8_t>& aData, StructuredCloneClosure& aClosure)
{
  auto* data = reinterpret_cast<uint64_t*>(aData.Elements());
  size_t dataLen = aData.Length();
  MOZ_ASSERT(!(dataLen % sizeof(*data)));

  JS_ClearStructuredClone(data, dataLen, &gCallbacks, &aClosure, false);
  aData.Clear();
}

} // namespace messageport
} // namespace dom
} // namespace mozilla
