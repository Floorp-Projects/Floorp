/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneUtils.h"

#include "nsIDOMFile.h"
#include "nsIDOMDOMException.h"
#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "nsThreadUtils.h"
#include "StructuredCloneTags.h"
#include "jsapi.h"

using namespace mozilla::dom;

namespace {

void
Error(JSContext* aCx, uint32_t aErrorId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_DOMStructuredCloneError(aCx, aErrorId);
}

JSObject*
Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
     uint32_t aData, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aClosure);

  StructuredCloneClosure* closure =
    static_cast<StructuredCloneClosure*>(aClosure);

  if (aTag == SCTAG_DOM_FILE) {
    MOZ_ASSERT(aData < closure->mBlobs.Length());

    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(closure->mBlobs[aData]);
    MOZ_ASSERT(file);

#ifdef DEBUG
    {
      // File should not be mutable.
      nsCOMPtr<nsIMutable> mutableFile = do_QueryInterface(file);
      bool isMutable;
      MOZ_ASSERT(NS_SUCCEEDED(mutableFile->GetMutable(&isMutable)));
      MOZ_ASSERT(!isMutable);
    }
#endif

    JS::Rooted<JS::Value> wrappedFile(aCx);
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    nsresult rv = nsContentUtils::WrapNative(aCx, global, file,
                                             &NS_GET_IID(nsIDOMFile),
                                             wrappedFile.address());
    if (NS_FAILED(rv)) {
      Error(aCx, nsIDOMDOMException::DATA_CLONE_ERR);
      return nullptr;
    }

    return &wrappedFile.toObject();
  }

  if (aTag == SCTAG_DOM_BLOB) {
    MOZ_ASSERT(aData < closure->mBlobs.Length());

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(closure->mBlobs[aData]);
    MOZ_ASSERT(blob);

#ifdef DEBUG
    {
      // Blob should not be mutable.
      nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
      bool isMutable;
      MOZ_ASSERT(NS_SUCCEEDED(mutableBlob->GetMutable(&isMutable)));
      MOZ_ASSERT(!isMutable);
    }
#endif

    JS::Rooted<JS::Value> wrappedBlob(aCx);
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    nsresult rv = nsContentUtils::WrapNative(aCx, global, blob,
                                             &NS_GET_IID(nsIDOMBlob),
                                             wrappedBlob.address());
    if (NS_FAILED(rv)) {
      Error(aCx, nsIDOMDOMException::DATA_CLONE_ERR);
      return nullptr;
    }

    return &wrappedBlob.toObject();
  }

  return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
}

bool
Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
      JS::Handle<JSObject*> aObj, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aClosure);

  StructuredCloneClosure* closure =
    static_cast<StructuredCloneClosure*>(aClosure);

  // See if this is a wrapped native.
  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(aCx, aObj, getter_AddRefs(wrappedNative));

  if (wrappedNative) {
    // Get the raw nsISupports out of it.
    nsISupports* wrappedObject = wrappedNative->Native();
    MOZ_ASSERT(wrappedObject);

    // See if the wrapped native is a nsIDOMFile.
    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(wrappedObject);
    if (file) {
      nsCOMPtr<nsIMutable> mutableFile = do_QueryInterface(file);
      if (mutableFile &&
          NS_SUCCEEDED(mutableFile->SetMutable(false)) &&
          JS_WriteUint32Pair(aWriter, SCTAG_DOM_FILE,
                             closure->mBlobs.Length())) {
        closure->mBlobs.AppendElement(file);
        return true;
      }
    }

    // See if the wrapped native is a nsIDOMBlob.
    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(wrappedObject);
    if (blob) {
      nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
      if (mutableBlob &&
          NS_SUCCEEDED(mutableBlob->SetMutable(false)) &&
          JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                             closure->mBlobs.Length())) {
        closure->mBlobs.AppendElement(blob);
        return true;
      }
    }
  }

  return NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr);
}

JSStructuredCloneCallbacks gCallbacks = {
  Read,
  Write,
  Error
};

} // anonymous namespace

namespace mozilla {
namespace dom {

bool
ReadStructuredClone(JSContext* aCx, uint64_t* aData, size_t aDataLength,
                    const StructuredCloneClosure& aClosure, JS::Value* aClone)
{
  void* closure = &const_cast<StructuredCloneClosure&>(aClosure);
  return !!JS_ReadStructuredClone(aCx, aData, aDataLength,
                                  JS_STRUCTURED_CLONE_VERSION, aClone,
                                  &gCallbacks, closure);
}

bool
WriteStructuredClone(JSContext* aCx, const JS::Value& aSource,
                     JSAutoStructuredCloneBuffer& aBuffer,
                     StructuredCloneClosure& aClosure)
{
  return aBuffer.write(aCx, aSource, &gCallbacks, &aClosure);
}

} // namespace dom
} // namespace mozilla
