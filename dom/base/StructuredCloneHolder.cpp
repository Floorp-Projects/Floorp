/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StructuredCloneHolder.h"

#include <new>
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "js/CallArgs.h"
#include "js/Value.h"
#include "js/WasmModule.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ClonedErrorHolder.h"
#include "mozilla/dom/ClonedErrorHolderBinding.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/FormDataBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/OffscreenCanvasBinding.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneBlob.h"
#include "mozilla/dom/StructuredCloneHolderBinding.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/TransformStream.h"
#include "mozilla/dom/TransformStreamBinding.h"
#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/dom/WebIDLSerializable.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WritableStreamBinding.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/fallible.h"
#include "mozilla/gfx/2D.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsID.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsIInputStream.h"
#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsJSPrincipals.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "xpcpublic.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

namespace {

JSObject* StructuredCloneCallbacksRead(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aIndex,
    void* aClosure) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomReadHandler(aCx, aReader, aCloneDataPolicy, aTag,
                                   aIndex);
}

bool StructuredCloneCallbacksWrite(JSContext* aCx,
                                   JSStructuredCloneWriter* aWriter,
                                   JS::Handle<JSObject*> aObj,
                                   bool* aSameProcessScopeRequired,
                                   void* aClosure) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomWriteHandler(aCx, aWriter, aObj,
                                    aSameProcessScopeRequired);
}

bool StructuredCloneCallbacksReadTransfer(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, void* aContent,
    uint64_t aExtraData, void* aClosure,
    JS::MutableHandle<JSObject*> aReturnObject) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomReadTransferHandler(aCx, aReader, aCloneDataPolicy, aTag,
                                           aContent, aExtraData, aReturnObject);
}

bool StructuredCloneCallbacksWriteTransfer(
    JSContext* aCx, JS::Handle<JSObject*> aObj, void* aClosure,
    // Output:
    uint32_t* aTag, JS::TransferableOwnership* aOwnership, void** aContent,
    uint64_t* aExtraData) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomWriteTransferHandler(aCx, aObj, aTag, aOwnership,
                                            aContent, aExtraData);
}

void StructuredCloneCallbacksFreeTransfer(uint32_t aTag,
                                          JS::TransferableOwnership aOwnership,
                                          void* aContent, uint64_t aExtraData,
                                          void* aClosure) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomFreeTransferHandler(aTag, aOwnership, aContent,
                                           aExtraData);
}

bool StructuredCloneCallbacksCanTransfer(JSContext* aCx,
                                         JS::Handle<JSObject*> aObject,
                                         bool* aSameProcessScopeRequired,
                                         void* aClosure) {
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->CustomCanTransferHandler(aCx, aObject,
                                          aSameProcessScopeRequired);
}

bool StructuredCloneCallbacksSharedArrayBuffer(JSContext* cx, bool aReceiving,
                                               void* aClosure) {
  if (!StaticPrefs::dom_workers_serialized_sab_access()) {
    return true;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

  if (workerPrivate) {
    workerPrivate->SetExecutionManager(
        JSExecutionManager::GetSABSerializationManager());
  } else if (NS_IsMainThread()) {
    nsIGlobalObject* global = GetCurrentGlobal();

    nsPIDOMWindowInner* innerWindow = nullptr;
    if (global) {
      innerWindow = global->GetAsInnerWindow();
    }

    DocGroup* docGroup = nullptr;
    if (innerWindow) {
      docGroup = innerWindow->GetDocGroup();
    }

    if (docGroup) {
      docGroup->SetExecutionManager(
          JSExecutionManager::GetSABSerializationManager());
    }
  }
  return true;
}

void StructuredCloneCallbacksError(JSContext* aCx, uint32_t aErrorId,
                                   void* aClosure, const char* aErrorMessage) {
  NS_WARNING("Failed to clone data.");
  StructuredCloneHolderBase* holder =
      static_cast<StructuredCloneHolderBase*>(aClosure);
  MOZ_ASSERT(holder);
  return holder->SetErrorMessage(aErrorMessage);
}

void AssertTagValues() {
  static_assert(SCTAG_DOM_IMAGEDATA == 0xffff8007 &&
                    SCTAG_DOM_DOMPOINT == 0xffff8008 &&
                    SCTAG_DOM_DOMPOINTREADONLY == 0xffff8009 &&
                    SCTAG_DOM_CRYPTOKEY == 0xffff800a &&
                    SCTAG_DOM_NULL_PRINCIPAL == 0xffff800b &&
                    SCTAG_DOM_SYSTEM_PRINCIPAL == 0xffff800c &&
                    SCTAG_DOM_CONTENT_PRINCIPAL == 0xffff800d &&
                    SCTAG_DOM_DOMQUAD == 0xffff800e &&
                    SCTAG_DOM_RTCCERTIFICATE == 0xffff800f &&
                    SCTAG_DOM_DOMRECT == 0xffff8010 &&
                    SCTAG_DOM_DOMRECTREADONLY == 0xffff8011 &&
                    SCTAG_DOM_EXPANDED_PRINCIPAL == 0xffff8012 &&
                    SCTAG_DOM_DOMMATRIX == 0xffff8013 &&
                    SCTAG_DOM_URLSEARCHPARAMS == 0xffff8014 &&
                    SCTAG_DOM_DOMMATRIXREADONLY == 0xffff8015 &&
                    SCTAG_DOM_STRUCTUREDCLONETESTER == 0xffff8018 &&
                    SCTAG_DOM_FILESYSTEMHANDLE == 0xffff8019 &&
                    SCTAG_DOM_FILESYSTEMFILEHANDLE == 0xffff801a &&
                    SCTAG_DOM_FILESYSTEMDIRECTORYHANDLE == 0xffff801b,
                "Something has changed the sctag values. This is wrong!");
}

}  // anonymous namespace

const JSStructuredCloneCallbacks StructuredCloneHolder::sCallbacks = {
    StructuredCloneCallbacksRead,
    StructuredCloneCallbacksWrite,
    StructuredCloneCallbacksError,
    StructuredCloneCallbacksReadTransfer,
    StructuredCloneCallbacksWriteTransfer,
    StructuredCloneCallbacksFreeTransfer,
    StructuredCloneCallbacksCanTransfer,
    StructuredCloneCallbacksSharedArrayBuffer,
};

// StructuredCloneHolderBase class

StructuredCloneHolderBase::StructuredCloneHolderBase(
    StructuredCloneScope aScope)
    : mStructuredCloneScope(aScope)
#ifdef DEBUG
      ,
      mClearCalled(false)
#endif
{
}

StructuredCloneHolderBase::~StructuredCloneHolderBase() {
#ifdef DEBUG
  MOZ_ASSERT(mClearCalled);
#endif
}

void StructuredCloneHolderBase::Clear() {
#ifdef DEBUG
  mClearCalled = true;
#endif

  mBuffer = nullptr;
}

bool StructuredCloneHolderBase::Write(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue) {
  return Write(aCx, aValue, JS::UndefinedHandleValue, JS::CloneDataPolicy());
}

bool StructuredCloneHolderBase::Write(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    JS::Handle<JS::Value> aTransfer,
    const JS::CloneDataPolicy& aCloneDataPolicy) {
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");
  MOZ_ASSERT(!mClearCalled, "This method cannot be called after Clear.");

  mBuffer = MakeUnique<JSAutoStructuredCloneBuffer>(
      mStructuredCloneScope, &StructuredCloneHolder::sCallbacks, this);

  if (!mBuffer->write(aCx, aValue, aTransfer, aCloneDataPolicy,
                      &StructuredCloneHolder::sCallbacks, this)) {
    mBuffer = nullptr;
    return false;
  }

  // Let's update our scope to the final one. The new one could be more
  // restrictive of the current one.
  MOZ_ASSERT(mStructuredCloneScope >= mBuffer->scope());
  mStructuredCloneScope = mBuffer->scope();
  return true;
}

bool StructuredCloneHolderBase::Read(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aValue) {
  return Read(aCx, aValue, JS::CloneDataPolicy());
}

bool StructuredCloneHolderBase::Read(
    JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
    const JS::CloneDataPolicy& aCloneDataPolicy) {
  MOZ_ASSERT(mBuffer, "Read() without Write() is not allowed.");
  MOZ_ASSERT(!mClearCalled, "This method cannot be called after Clear.");

  bool ok = mBuffer->read(aCx, aValue, aCloneDataPolicy,
                          &StructuredCloneHolder::sCallbacks, this);
  return ok;
}

bool StructuredCloneHolderBase::CustomReadTransferHandler(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, void* aContent,
    uint64_t aExtraData, JS::MutableHandle<JSObject*> aReturnObject) {
  MOZ_CRASH("Nothing to read.");
  return false;
}

bool StructuredCloneHolderBase::CustomWriteTransferHandler(
    JSContext* aCx, JS::Handle<JSObject*> aObj, uint32_t* aTag,
    JS::TransferableOwnership* aOwnership, void** aContent,
    uint64_t* aExtraData) {
  // No transfers are supported by default.
  return false;
}

void StructuredCloneHolderBase::CustomFreeTransferHandler(
    uint32_t aTag, JS::TransferableOwnership aOwnership, void* aContent,
    uint64_t aExtraData) {
  MOZ_CRASH("Nothing to free.");
}

bool StructuredCloneHolderBase::CustomCanTransferHandler(
    JSContext* aCx, JS::Handle<JSObject*> aObj,
    bool* aSameProcessScopeRequired) {
  return false;
}

// StructuredCloneHolder class

StructuredCloneHolder::StructuredCloneHolder(
    CloningSupport aSupportsCloning, TransferringSupport aSupportsTransferring,
    StructuredCloneScope aScope)
    : StructuredCloneHolderBase(aScope),
      mSupportsCloning(aSupportsCloning == CloningSupported),
      mSupportsTransferring(aSupportsTransferring == TransferringSupported),
      mGlobal(nullptr)
#ifdef DEBUG
      ,
      mCreationEventTarget(GetCurrentSerialEventTarget())
#endif
{
}

StructuredCloneHolder::~StructuredCloneHolder() {
  Clear();
  MOZ_ASSERT(mTransferredPorts.IsEmpty());
}

void StructuredCloneHolder::Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                  ErrorResult& aRv) {
  Write(aCx, aValue, JS::UndefinedHandleValue, JS::CloneDataPolicy(), aRv);
}

void StructuredCloneHolder::Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                  JS::Handle<JS::Value> aTransfer,
                                  const JS::CloneDataPolicy& aCloneDataPolicy,
                                  ErrorResult& aRv) {
  if (!StructuredCloneHolderBase::Write(aCx, aValue, aTransfer,
                                        aCloneDataPolicy)) {
    aRv.ThrowDataCloneError(mErrorMessage);
    return;
  }
}

void StructuredCloneHolder::Read(nsIGlobalObject* aGlobal, JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aValue,
                                 ErrorResult& aRv) {
  return Read(aGlobal, aCx, aValue, JS::CloneDataPolicy(), aRv);
}

void StructuredCloneHolder::Read(nsIGlobalObject* aGlobal, JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aValue,
                                 const JS::CloneDataPolicy& aCloneDataPolicy,
                                 ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  mozilla::AutoRestore<nsIGlobalObject*> guard(mGlobal);
  auto errorMessageGuard = MakeScopeExit([&] { mErrorMessage.Truncate(); });
  mGlobal = aGlobal;

  if (!StructuredCloneHolderBase::Read(aCx, aValue, aCloneDataPolicy)) {
    JS_ClearPendingException(aCx);
    aRv.ThrowDataCloneError(mErrorMessage);
    return;
  }

  // If we are tranferring something, we cannot call 'Read()' more than once.
  if (mSupportsTransferring) {
    mBlobImplArray.Clear();
    mWasmModuleArray.Clear();
    mClonedSurfaces.Clear();
    mInputStreamArray.Clear();
    mVideoFrames.Clear();
    mEncodedVideoChunks.Clear();
    Clear();
  }
}

void StructuredCloneHolder::ReadFromBuffer(
    nsIGlobalObject* aGlobal, JSContext* aCx, JSStructuredCloneData& aBuffer,
    JS::MutableHandle<JS::Value> aValue,
    const JS::CloneDataPolicy& aCloneDataPolicy, ErrorResult& aRv) {
  ReadFromBuffer(aGlobal, aCx, aBuffer, JS_STRUCTURED_CLONE_VERSION, aValue,
                 aCloneDataPolicy, aRv);
}

void StructuredCloneHolder::ReadFromBuffer(
    nsIGlobalObject* aGlobal, JSContext* aCx, JSStructuredCloneData& aBuffer,
    uint32_t aAlgorithmVersion, JS::MutableHandle<JS::Value> aValue,
    const JS::CloneDataPolicy& aCloneDataPolicy, ErrorResult& aRv) {
  MOZ_ASSERT(!mBuffer, "ReadFromBuffer() must be called without a Write().");

  mozilla::AutoRestore<nsIGlobalObject*> guard(mGlobal);
  auto errorMessageGuard = MakeScopeExit([&] { mErrorMessage.Truncate(); });
  mGlobal = aGlobal;

  if (!JS_ReadStructuredClone(aCx, aBuffer, aAlgorithmVersion, CloneScope(),
                              aValue, aCloneDataPolicy, &sCallbacks, this)) {
    JS_ClearPendingException(aCx);
    aRv.ThrowDataCloneError(mErrorMessage);
    return;
  }
}

static bool CheckExposedGlobals(JSContext* aCx, nsIGlobalObject* aGlobal,
                                uint16_t aExposedGlobals) {
  JS::Rooted<JSObject*> global(aCx, aGlobal->GetGlobalJSObject());

  // Sandboxes aren't really DOM globals (though they do set the
  // JSCLASS_DOM_GLOBAL flag), and so we can't simply do the exposure check.
  // Some sandboxes do have a DOM global as their prototype, so using the
  // prototype to check for exposure will at least make it work for those
  // specific cases.
  {
    JSObject* proto = xpc::SandboxPrototypeOrNull(aCx, global);
    if (proto) {
      global = proto;
    }
  }

  if (!IsGlobalInExposureSet(aCx, global, aExposedGlobals)) {
    ErrorResult error;
    error.ThrowDataCloneError("Interface is not exposed.");
    MOZ_ALWAYS_TRUE(error.MaybeSetPendingException(aCx));
    return false;
  }
  return true;
}

/* static */
JSObject* StructuredCloneHolder::ReadFullySerializableObjects(
    JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
    bool aIsForIndexedDB) {
  AssertTagValues();

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (!global) {
    return nullptr;
  }

  Maybe<std::pair<uint16_t, WebIDLDeserializer>> deserializer =
      LookupDeserializer(StructuredCloneTags(aTag));
  if (deserializer.isSome()) {
    uint16_t exposedGlobals;
    WebIDLDeserializer deserialize;
    std::tie(exposedGlobals, deserialize) = deserializer.ref();

    // https://html.spec.whatwg.org/multipage/structured-data.html#structureddeserialize
    //
    // 22. Otherwise:
    //
    //   1. Let interfaceName be serialized.[[Type]].
    //   2. If the interface identified by interfaceName is not exposed in
    //      targetRealm, then throw a "DataCloneError" DOMException.
    //
    // The special-casing for IndexedDB is because it uses a sandbox to
    // deserialize, which means we don't actually have access to exposure
    // information.
    if (!aIsForIndexedDB && !CheckExposedGlobals(aCx, global, exposedGlobals)) {
      return nullptr;
    }

    return deserialize(aCx, global, aReader);
  }

  if (aTag == SCTAG_DOM_NULL_PRINCIPAL || aTag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
      aTag == SCTAG_DOM_CONTENT_PRINCIPAL ||
      aTag == SCTAG_DOM_EXPANDED_PRINCIPAL) {
    JSPrincipals* prin;
    if (!nsJSPrincipals::ReadKnownPrincipalType(aCx, aReader, aTag, &prin)) {
      return nullptr;
    }

    JS::Rooted<JS::Value> result(aCx);
    {
      // nsJSPrincipals::ReadKnownPrincipalType addrefs for us, but because of
      // the casting between JSPrincipals* and nsIPrincipal* we can't use
      // getter_AddRefs above and have to already_AddRefed here.
      nsCOMPtr<nsIPrincipal> principal =
          already_AddRefed<nsIPrincipal>(nsJSPrincipals::get(prin));

      nsresult rv = nsContentUtils::WrapNative(
          aCx, principal, &NS_GET_IID(nsIPrincipal), &result);
      if (NS_FAILED(rv)) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return nullptr;
      }
    }
    return result.toObjectOrNull();
  }

  // Don't know what this is. Bail.
  xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nullptr;
}

/* static */
bool StructuredCloneHolder::WriteFullySerializableObjects(
    JSContext* aCx, JSStructuredCloneWriter* aWriter,
    JS::Handle<JSObject*> aObj) {
  AssertTagValues();

  // Window and Location are not serializable, so it's OK to just do a static
  // unwrap here.
  JS::Rooted<JSObject*> obj(aCx, js::CheckedUnwrapStatic(aObj));
  if (!obj) {
    return xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  }

  const DOMJSClass* domClass = GetDOMClass(obj);
  if (domClass && domClass->mSerializer) {
    return domClass->mSerializer(aCx, aWriter, obj);
  }

  if (NS_IsMainThread() && xpc::IsReflector(obj, aCx)) {
    // We only care about principals, so ReflectorToISupportsStatic is fine.
    nsCOMPtr<nsISupports> base = xpc::ReflectorToISupportsStatic(obj);
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(base);
    if (principal) {
      auto nsjsprincipals = nsJSPrincipals::get(principal);
      return nsjsprincipals->write(aCx, aWriter);
    }
  }

  // Don't know what this is
  ErrorResult rv;
  const char* className = JS::GetClass(obj)->name;
  rv.ThrowDataCloneError(nsDependentCString(className) +
                         " object could not be cloned."_ns);
  MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(aCx));
  return false;
}

template <typename char_type>
static bool ReadTString(JSStructuredCloneReader* aReader,
                        nsTString<char_type>& aString) {
  uint32_t length, zero;
  if (!JS_ReadUint32Pair(aReader, &length, &zero)) {
    return false;
  }

  if (NS_WARN_IF(!aString.SetLength(length, fallible))) {
    return false;
  }
  size_t charSize = sizeof(char_type);
  return JS_ReadBytes(aReader, (void*)aString.BeginWriting(),
                      length * charSize);
}

template <typename char_type>
static bool WriteTString(JSStructuredCloneWriter* aWriter,
                         const nsTSubstring<char_type>& aString) {
  size_t charSize = sizeof(char_type);
  return JS_WriteUint32Pair(aWriter, aString.Length(), 0) &&
         JS_WriteBytes(aWriter, aString.BeginReading(),
                       aString.Length() * charSize);
}

/* static */
bool StructuredCloneHolder::ReadString(JSStructuredCloneReader* aReader,
                                       nsString& aString) {
  return ReadTString(aReader, aString);
}

/* static */
bool StructuredCloneHolder::WriteString(JSStructuredCloneWriter* aWriter,
                                        const nsAString& aString) {
  return WriteTString(aWriter, aString);
}

/* static */
bool StructuredCloneHolder::ReadCString(JSStructuredCloneReader* aReader,
                                        nsCString& aString) {
  return ReadTString(aReader, aString);
}

/* static */
bool StructuredCloneHolder::WriteCString(JSStructuredCloneWriter* aWriter,
                                         const nsACString& aString) {
  return WriteTString(aWriter, aString);
}

namespace {

JSObject* ReadBlob(JSContext* aCx, uint32_t aIndex,
                   StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aHolder);
#ifdef FUZZING
  if (aIndex >= aHolder->BlobImpls().Length()) {
    return nullptr;
  }
#endif
  MOZ_ASSERT(aIndex < aHolder->BlobImpls().Length());
  JS::Rooted<JS::Value> val(aCx);
  {
    // RefPtr<File> and RefPtr<BlobImpl> need to go out of scope before
    // toObject() is called because the static analysis thinks releasing XPCOM
    // objects can GC (because in some cases it can!), and a return statement
    // with a JSObject* type means that JSObject* is on the stack as a raw
    // pointer while destructors are running.
    RefPtr<BlobImpl> blobImpl = aHolder->BlobImpls()[aIndex];

    RefPtr<Blob> blob = Blob::Create(aHolder->GlobalDuringRead(), blobImpl);
    if (NS_WARN_IF(!blob)) {
      return nullptr;
    }

    if (!ToJSValue(aCx, blob, &val)) {
      return nullptr;
    }
  }

  return &val.toObject();
}

bool WriteBlob(JSStructuredCloneWriter* aWriter, Blob* aBlob,
               StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(aHolder);

  RefPtr<BlobImpl> blobImpl = aBlob->Impl();

  // We store the position of the blobImpl in the array as index.
  if (JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                         aHolder->BlobImpls().Length())) {
    aHolder->BlobImpls().AppendElement(blobImpl);
    return true;
  }

  return false;
}

// A directory is serialized as:
// - pair of ints: SCTAG_DOM_DIRECTORY, path length
// - path as string
bool WriteDirectory(JSStructuredCloneWriter* aWriter, Directory* aDirectory) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aDirectory);

  nsAutoString path;
  aDirectory->GetFullRealPath(path);

  size_t charSize = sizeof(nsString::char_type);
  return JS_WriteUint32Pair(aWriter, SCTAG_DOM_DIRECTORY, path.Length()) &&
         JS_WriteBytes(aWriter, path.get(), path.Length() * charSize);
}

already_AddRefed<Directory> ReadDirectoryInternal(
    JSStructuredCloneReader* aReader, uint32_t aPathLength,
    StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(aHolder);

  nsAutoString path;
  if (NS_WARN_IF(!path.SetLength(aPathLength, fallible))) {
    return nullptr;
  }
  size_t charSize = sizeof(nsString::char_type);
  if (!JS_ReadBytes(aReader, (void*)path.BeginWriting(),
                    aPathLength * charSize)) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(path, true, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  RefPtr<Directory> directory =
      Directory::Create(aHolder->GlobalDuringRead(), file);
  return directory.forget();
}

JSObject* ReadDirectory(JSContext* aCx, JSStructuredCloneReader* aReader,
                        uint32_t aPathLength, StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(aHolder);

  // RefPtr<Directory> needs to go out of scope before toObject() is
  // called because the static analysis thinks dereferencing XPCOM objects
  // can GC (because in some cases it can!), and a return statement with a
  // JSObject* type means that JSObject* is on the stack as a raw pointer
  // while destructors are running.
  JS::Rooted<JS::Value> val(aCx);
  {
    RefPtr<Directory> directory =
        ReadDirectoryInternal(aReader, aPathLength, aHolder);
    if (!directory) {
      return nullptr;
    }

    if (!ToJSValue(aCx, directory, &val)) {
      return nullptr;
    }
  }

  return &val.toObject();
}

// Read the WriteFileList for the format.
JSObject* ReadFileList(JSContext* aCx, JSStructuredCloneReader* aReader,
                       uint32_t aCount, StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);

  JS::Rooted<JS::Value> val(aCx);
  {
    RefPtr<FileList> fileList = new FileList(aHolder->GlobalDuringRead());

    uint32_t zero, index;
    // |index| is the index of the first blobImpl.
    if (!JS_ReadUint32Pair(aReader, &zero, &index) || zero != 0) {
      return nullptr;
    }

    // |aCount| is the number of BlobImpls to use from the |index|.
    for (uint32_t i = 0; i < aCount; ++i) {
      uint32_t pos = index + i;
#ifdef FUZZING
      if (pos >= aHolder->BlobImpls().Length()) {
        return nullptr;
      }
#endif
      MOZ_ASSERT(pos < aHolder->BlobImpls().Length());

      RefPtr<BlobImpl> blobImpl = aHolder->BlobImpls()[pos];
      MOZ_ASSERT(blobImpl->IsFile());

      RefPtr<File> file = File::Create(aHolder->GlobalDuringRead(), blobImpl);
      if (NS_WARN_IF(!file)) {
        return nullptr;
      }

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

// The format of the FileList serialization is:
// - pair of ints: SCTAG_DOM_FILELIST, Length of the FileList
// - pair of ints: 0, The offset of the BlobImpl array
bool WriteFileList(JSStructuredCloneWriter* aWriter, FileList* aFileList,
                   StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aFileList);
  MOZ_ASSERT(aHolder);

  // A FileList is serialized writing the X number of elements and the offset
  // from mBlobImplArray. The Read will take X elements from mBlobImplArray
  // starting from the offset.
  if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_FILELIST, aFileList->Length()) ||
      !JS_WriteUint32Pair(aWriter, 0, aHolder->BlobImpls().Length())) {
    return false;
  }

  nsTArray<RefPtr<BlobImpl>> blobImpls;

  for (uint32_t i = 0; i < aFileList->Length(); ++i) {
    RefPtr<BlobImpl> blobImpl = aFileList->Item(i)->Impl();
    blobImpls.AppendElement(blobImpl);
  }

  aHolder->BlobImpls().AppendElements(blobImpls);
  return true;
}

// Read the WriteFormData for the format.
JSObject* ReadFormData(JSContext* aCx, JSStructuredCloneReader* aReader,
                       uint32_t aCount, StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(aHolder);

  // See the serialization of the FormData for the format.
  JS::Rooted<JS::Value> val(aCx);
  {
    RefPtr<FormData> formData = new FormData(aHolder->GlobalDuringRead());

    Optional<nsAString> thirdArg;
    for (uint32_t i = 0; i < aCount; ++i) {
      nsAutoString name;
      if (!StructuredCloneHolder::ReadString(aReader, name)) {
        return nullptr;
      }

      uint32_t tag, indexOrLengthOfString;
      if (!JS_ReadUint32Pair(aReader, &tag, &indexOrLengthOfString)) {
        return nullptr;
      }

      if (tag == SCTAG_DOM_BLOB) {
#ifdef FUZZING
        if (indexOrLengthOfString >= aHolder->BlobImpls().Length()) {
          return nullptr;
        }
#endif
        MOZ_ASSERT(indexOrLengthOfString < aHolder->BlobImpls().Length());

        RefPtr<BlobImpl> blobImpl = aHolder->BlobImpls()[indexOrLengthOfString];

        RefPtr<Blob> blob = Blob::Create(aHolder->GlobalDuringRead(), blobImpl);
        if (NS_WARN_IF(!blob)) {
          return nullptr;
        }

        ErrorResult rv;
        formData->Append(name, *blob, thirdArg, rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          return nullptr;
        }

      } else if (tag == SCTAG_DOM_DIRECTORY) {
        RefPtr<Directory> directory =
            ReadDirectoryInternal(aReader, indexOrLengthOfString, aHolder);
        if (!directory) {
          return nullptr;
        }

        formData->Append(name, directory);

      } else {
        if (NS_WARN_IF(tag != 0)) {
          return nullptr;
        }

        nsAutoString value;
        if (NS_WARN_IF(!value.SetLength(indexOrLengthOfString, fallible))) {
          return nullptr;
        }
        size_t charSize = sizeof(nsString::char_type);
        if (!JS_ReadBytes(aReader, (void*)value.BeginWriting(),
                          indexOrLengthOfString * charSize)) {
          return nullptr;
        }

        ErrorResult rv;
        formData->Append(name, value, rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          return nullptr;
        }
      }
    }

    if (!ToJSValue(aCx, formData, &val)) {
      return nullptr;
    }
  }

  return &val.toObject();
}

// The format of the FormData serialization is:
// - pair of ints: SCTAG_DOM_FORMDATA, Length of the FormData elements
// - for each Element element:
//   - name string
//   - if it's a blob:
//     - pair of ints: SCTAG_DOM_BLOB, index of the BlobImpl in the array
//       mBlobImplArray.
//   - if it's a directory (See WriteDirectory):
//     - pair of ints: SCTAG_DOM_DIRECTORY, path length
//     - path as string
//   - else:
//     - pair of ints: 0, string length
//     - value string
bool WriteFormData(JSStructuredCloneWriter* aWriter, FormData* aFormData,
                   StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aFormData);
  MOZ_ASSERT(aHolder);

  if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_FORMDATA, aFormData->Length())) {
    return false;
  }

  auto write = [aWriter, aHolder](
                   const nsString& aName,
                   const OwningBlobOrDirectoryOrUSVString& aValue) {
    if (!StructuredCloneHolder::WriteString(aWriter, aName)) {
      return false;
    }

    if (aValue.IsBlob()) {
      if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                              aHolder->BlobImpls().Length())) {
        return false;
      }

      RefPtr<BlobImpl> blobImpl = aValue.GetAsBlob()->Impl();

      aHolder->BlobImpls().AppendElement(blobImpl);
      return true;
    }

    if (aValue.IsDirectory()) {
      Directory* directory = aValue.GetAsDirectory();
      return WriteDirectory(aWriter, directory);
    }

    const size_t charSize = sizeof(nsString::char_type);
    return JS_WriteUint32Pair(aWriter, 0, aValue.GetAsUSVString().Length()) &&
           JS_WriteBytes(aWriter, aValue.GetAsUSVString().get(),
                         aValue.GetAsUSVString().Length() * charSize);
  };
  return aFormData->ForEach(write);
}

JSObject* ReadWasmModule(JSContext* aCx, uint32_t aIndex,
                         StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aHolder);
  MOZ_ASSERT(aHolder->CloneScope() ==
             StructuredCloneHolder::StructuredCloneScope::SameProcess);
#ifdef FUZZING
  if (aIndex >= aHolder->WasmModules().Length()) {
    return nullptr;
  }
#endif
  MOZ_ASSERT(aIndex < aHolder->WasmModules().Length());

  return aHolder->WasmModules()[aIndex]->createObject(aCx);
}

bool WriteWasmModule(JSStructuredCloneWriter* aWriter,
                     JS::WasmModule* aWasmModule,
                     StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aWasmModule);
  MOZ_ASSERT(aHolder);
  MOZ_ASSERT(aHolder->CloneScope() ==
             StructuredCloneHolder::StructuredCloneScope::SameProcess);

  // We store the position of the wasmModule in the array as index.
  if (JS_WriteUint32Pair(aWriter, SCTAG_DOM_WASM_MODULE,
                         aHolder->WasmModules().Length())) {
    aHolder->WasmModules().AppendElement(aWasmModule);
    return true;
  }

  return false;
}

JSObject* ReadInputStream(JSContext* aCx, uint32_t aIndex,
                          StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aHolder);
#ifdef FUZZING
  if (aIndex >= aHolder->InputStreams().Length()) {
    return nullptr;
  }
#endif
  MOZ_ASSERT(aIndex < aHolder->InputStreams().Length());
  JS::Rooted<JS::Value> result(aCx);
  {
    nsCOMPtr<nsIInputStream> inputStream = aHolder->InputStreams()[aIndex];

    nsresult rv = nsContentUtils::WrapNative(
        aCx, inputStream, &NS_GET_IID(nsIInputStream), &result);
    if (NS_FAILED(rv)) {
      return nullptr;
    }
  }

  return &result.toObject();
}

bool WriteInputStream(JSStructuredCloneWriter* aWriter,
                      nsIInputStream* aInputStream,
                      StructuredCloneHolder* aHolder) {
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aHolder);

  // We store the position of the inputStream in the array as index.
  if (JS_WriteUint32Pair(aWriter, SCTAG_DOM_INPUTSTREAM,
                         aHolder->InputStreams().Length())) {
    aHolder->InputStreams().AppendElement(aInputStream);
    return true;
  }

  return false;
}

}  // anonymous namespace

static const uint16_t sWindowOrWorker =
    GlobalNames::DedicatedWorkerGlobalScope |
    GlobalNames::ServiceWorkerGlobalScope |
    GlobalNames::SharedWorkerGlobalScope | GlobalNames::Window |
    GlobalNames::WorkerDebuggerGlobalScope;

JSObject* StructuredCloneHolder::CustomReadHandler(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
    uint32_t aIndex) {
  MOZ_ASSERT(mSupportsCloning);

  if (aTag == SCTAG_DOM_BLOB) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    return ReadBlob(aCx, aIndex, this);
  }

  if (aTag == SCTAG_DOM_DIRECTORY) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    return ReadDirectory(aCx, aReader, aIndex, this);
  }

  if (aTag == SCTAG_DOM_FILELIST) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    return ReadFileList(aCx, aReader, aIndex, this);
  }

  if (aTag == SCTAG_DOM_FORMDATA) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    return ReadFormData(aCx, aReader, aIndex, this);
  }

  if (aTag == SCTAG_DOM_IMAGEBITMAP &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    // Get the current global object.
    // This can be null.
    JS::Rooted<JSObject*> result(aCx);
    {
      // aIndex is the index of the cloned image.
      result = ImageBitmap::ReadStructuredClone(aCx, aReader, mGlobal,
                                                GetSurfaces(), aIndex);
    }
    return result;
  }

  if (aTag == SCTAG_DOM_STRUCTURED_CLONE_HOLDER) {
    return StructuredCloneBlob::ReadStructuredClone(aCx, aReader, this);
  }

  if (aTag == SCTAG_DOM_WASM_MODULE &&
      CloneScope() == StructuredCloneScope::SameProcess &&
      aCloneDataPolicy.areIntraClusterClonableSharedObjectsAllowed()) {
    return ReadWasmModule(aCx, aIndex, this);
  }

  if (aTag == SCTAG_DOM_INPUTSTREAM) {
    return ReadInputStream(aCx, aIndex, this);
  }

  if (aTag == SCTAG_DOM_BROWSING_CONTEXT) {
    if (!CheckExposedGlobals(aCx, mGlobal, GlobalNames::Window)) {
      return nullptr;
    }
    return BrowsingContext::ReadStructuredClone(aCx, aReader, this);
  }

  if (aTag == SCTAG_DOM_CLONED_ERROR_OBJECT) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return nullptr;
    }
    return ClonedErrorHolder::ReadStructuredClone(aCx, aReader, this);
  }

  if (StaticPrefs::dom_media_webcodecs_enabled() &&
      aTag == SCTAG_DOM_VIDEOFRAME &&
      CloneScope() == StructuredCloneScope::SameProcess &&
      aCloneDataPolicy.areIntraClusterClonableSharedObjectsAllowed()) {
    JS::Rooted<JSObject*> global(aCx, mGlobal->GetGlobalJSObject());
    if (VideoFrame_Binding::ConstructorEnabled(aCx, global)) {
      return VideoFrame::ReadStructuredClone(aCx, mGlobal, aReader,
                                             VideoFrames()[aIndex]);
    }
  }

  if (StaticPrefs::dom_media_webcodecs_enabled() &&
      aTag == SCTAG_DOM_ENCODEDVIDEOCHUNK &&
      CloneScope() == StructuredCloneScope::SameProcess &&
      aCloneDataPolicy.areIntraClusterClonableSharedObjectsAllowed()) {
    JS::Rooted<JSObject*> global(aCx, mGlobal->GetGlobalJSObject());
    if (EncodedVideoChunk_Binding::ConstructorEnabled(aCx, global)) {
      return EncodedVideoChunk::ReadStructuredClone(
          aCx, mGlobal, aReader, EncodedVideoChunks()[aIndex]);
    }
  }

  return ReadFullySerializableObjects(aCx, aReader, aTag, false);
}

bool StructuredCloneHolder::CustomWriteHandler(
    JSContext* aCx, JSStructuredCloneWriter* aWriter,
    JS::Handle<JSObject*> aObj, bool* aSameProcessScopeRequired) {
  if (!mSupportsCloning) {
    return false;
  }

  JS::Rooted<JSObject*> obj(aCx, aObj);

  // See if this is a File/Blob object.
  {
    Blob* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, &obj, blob))) {
      return WriteBlob(aWriter, blob, this);
    }
  }

  // See if this is a Directory object.
  {
    Directory* directory = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Directory, &obj, directory))) {
      return WriteDirectory(aWriter, directory);
    }
  }

  // See if this is a FileList object.
  {
    FileList* fileList = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FileList, &obj, fileList))) {
      return WriteFileList(aWriter, fileList, this);
    }
  }

  // See if this is a FormData object.
  {
    FormData* formData = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FormData, &obj, formData))) {
      return WriteFormData(aWriter, formData, this);
    }
  }

  // See if this is an ImageBitmap object.
  {
    ImageBitmap* imageBitmap = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageBitmap, &obj, imageBitmap))) {
      SameProcessScopeRequired(aSameProcessScopeRequired);

      if (CloneScope() == StructuredCloneScope::SameProcess) {
        ErrorResult rv;
        ImageBitmap::WriteStructuredClone(aWriter, GetSurfaces(), imageBitmap,
                                          rv);
        return !rv.MaybeSetPendingException(aCx);
      }
      return false;
    }
  }

  // See if this is a StructuredCloneBlob object.
  {
    StructuredCloneBlob* holder = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(StructuredCloneHolder, &obj, holder))) {
      return holder->WriteStructuredClone(aCx, aWriter, this);
    }
  }

  // See if this is a BrowsingContext object.
  {
    BrowsingContext* holder = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(BrowsingContext, &obj, holder))) {
      return holder->WriteStructuredClone(aCx, aWriter, this);
    }
  }

  // See if this is a ClonedErrorHolder object.
  {
    ClonedErrorHolder* holder = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(ClonedErrorHolder, &obj, holder))) {
      return holder->WriteStructuredClone(aCx, aWriter, this);
    }
  }

  // See if this is a WasmModule.
  if (JS::IsWasmModuleObject(obj)) {
    SameProcessScopeRequired(aSameProcessScopeRequired);
    if (CloneScope() == StructuredCloneScope::SameProcess) {
      RefPtr<JS::WasmModule> module = JS::GetWasmModule(obj);
      MOZ_ASSERT(module);

      return WriteWasmModule(aWriter, module, this);
    }
    return false;
  }

  // See if this is a VideoFrame object.
  if (StaticPrefs::dom_media_webcodecs_enabled()) {
    VideoFrame* videoFrame = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(VideoFrame, &obj, videoFrame))) {
      SameProcessScopeRequired(aSameProcessScopeRequired);
      return CloneScope() == StructuredCloneScope::SameProcess
                 ? videoFrame->WriteStructuredClone(aWriter, this)
                 : false;
    }
  }

  // See if this is a EncodedVideoChunk object.
  if (StaticPrefs::dom_media_webcodecs_enabled()) {
    EncodedVideoChunk* encodedVideoChunk = nullptr;
    if (NS_SUCCEEDED(
            UNWRAP_OBJECT(EncodedVideoChunk, &obj, encodedVideoChunk))) {
      SameProcessScopeRequired(aSameProcessScopeRequired);
      return CloneScope() == StructuredCloneScope::SameProcess
                 ? encodedVideoChunk->WriteStructuredClone(aWriter, this)
                 : false;
    }
  }

  {
    // We only care about streams, so ReflectorToISupportsStatic is fine.
    nsCOMPtr<nsISupports> base = xpc::ReflectorToISupportsStatic(aObj);
    nsCOMPtr<nsIInputStream> inputStream = do_QueryInterface(base);
    if (inputStream) {
      return WriteInputStream(aWriter, inputStream, this);
    }
  }

  return WriteFullySerializableObjects(aCx, aWriter, aObj);
}

already_AddRefed<MessagePort> StructuredCloneHolder::ReceiveMessagePort(
    uint64_t aIndex) {
  if (NS_WARN_IF(aIndex >= mPortIdentifiers.Length())) {
    return nullptr;
  }
  UniqueMessagePortId portId(mPortIdentifiers[aIndex]);

  ErrorResult rv;
  RefPtr<MessagePort> port = MessagePort::Create(mGlobal, portId, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return nullptr;
  }

  return port.forget();
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
MOZ_CAN_RUN_SCRIPT_BOUNDARY bool
StructuredCloneHolder::CustomReadTransferHandler(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, void* aContent,
    uint64_t aExtraData, JS::MutableHandle<JSObject*> aReturnObject) {
  MOZ_ASSERT(mSupportsTransferring);

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    if (!CheckExposedGlobals(
            aCx, mGlobal,
            sWindowOrWorker | GlobalNames::AudioWorkletGlobalScope)) {
      return false;
    }
#ifdef FUZZING
    if (aExtraData >= mPortIdentifiers.Length()) {
      return false;
    }
#endif
    RefPtr<MessagePort> port = ReceiveMessagePort(aExtraData);
    if (!port) {
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

  if (aTag == SCTAG_DOM_CANVAS &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return false;
    }
    MOZ_ASSERT(aContent);
    OffscreenCanvasCloneData* data =
        static_cast<OffscreenCanvasCloneData*>(aContent);
    RefPtr<OffscreenCanvas> canvas =
        OffscreenCanvas::CreateFromCloneData(mGlobal, data);
    delete data;

    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, canvas, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    aReturnObject.set(&value.toObject());
    return true;
  }

  if (aTag == SCTAG_DOM_IMAGEBITMAP &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    if (!CheckExposedGlobals(aCx, mGlobal, sWindowOrWorker)) {
      return false;
    }
    MOZ_ASSERT(aContent);
    ImageBitmapCloneData* data = static_cast<ImageBitmapCloneData*>(aContent);
    RefPtr<ImageBitmap> bitmap =
        ImageBitmap::CreateFromCloneData(mGlobal, data);
    delete data;

    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, bitmap, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    aReturnObject.set(&value.toObject());
    return true;
  }

  if (aTag == SCTAG_DOM_READABLESTREAM) {
#ifdef FUZZING
    if (aExtraData >= mPortIdentifiers.Length()) {
      return false;
    }
#endif
    RefPtr<MessagePort> port = ReceiveMessagePort(aExtraData);
    if (!port) {
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = mGlobal;
    return ReadableStream::ReceiveTransfer(aCx, global, *port, aReturnObject);
  }

  if (aTag == SCTAG_DOM_WRITABLESTREAM) {
#ifdef FUZZING
    if (aExtraData >= mPortIdentifiers.Length()) {
      return false;
    }
#endif
    RefPtr<MessagePort> port = ReceiveMessagePort(aExtraData);
    if (!port) {
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = mGlobal;
    return WritableStream::ReceiveTransfer(aCx, global, *port, aReturnObject);
  }

  if (aTag == SCTAG_DOM_TRANSFORMSTREAM) {
#ifdef FUZZING
    if (aExtraData + 1 >= mPortIdentifiers.Length()) {
      return false;
    }
#endif
    RefPtr<MessagePort> port1 = ReceiveMessagePort(aExtraData);
    RefPtr<MessagePort> port2 = ReceiveMessagePort(aExtraData + 1);
    if (!port1 || !port2) {
      return false;
    }
    nsCOMPtr<nsIGlobalObject> global = mGlobal;
    return TransformStream::ReceiveTransfer(aCx, global, *port1, *port2,
                                            aReturnObject);
  }

  if (StaticPrefs::dom_media_webcodecs_enabled() &&
      aTag == SCTAG_DOM_VIDEOFRAME &&
      CloneScope() == StructuredCloneScope::SameProcess &&
      aCloneDataPolicy.areIntraClusterClonableSharedObjectsAllowed()) {
    MOZ_ASSERT(aContent);

    JS::Rooted<JSObject*> globalObj(aCx, mGlobal->GetGlobalJSObject());
    // aContent will be released in CustomFreeTransferHandler.
    if (!VideoFrame_Binding::ConstructorEnabled(aCx, globalObj)) {
      return false;
    }

    VideoFrame::TransferredData* data =
        static_cast<VideoFrame::TransferredData*>(aContent);
    nsCOMPtr<nsIGlobalObject> global = mGlobal;
    RefPtr<VideoFrame> frame = VideoFrame::FromTransferred(global.get(), data);
    // aContent will be released in CustomFreeTransferHandler if frame is null.
    if (!frame) {
      return false;
    }
    delete data;
    aContent = nullptr;

    JS::Rooted<JS::Value> value(aCx);
    if (!GetOrCreateDOMReflector(aCx, frame, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }
    aReturnObject.set(&value.toObject());
    return true;
  }

  return false;
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
MOZ_CAN_RUN_SCRIPT_BOUNDARY bool
StructuredCloneHolder::CustomWriteTransferHandler(
    JSContext* aCx, JS::Handle<JSObject*> aObj, uint32_t* aTag,
    JS::TransferableOwnership* aOwnership, void** aContent,
    uint64_t* aExtraData) {
  if (!mSupportsTransferring) {
    return false;
  }

  JS::Rooted<JSObject*> obj(aCx, aObj);

  {
    MessagePort* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, &obj, port);
    if (NS_SUCCEEDED(rv)) {
      if (!port->CanBeCloned()) {
        return false;
      }

      UniqueMessagePortId identifier;
      port->CloneAndDisentangle(identifier);

      // We use aExtraData to store the index of this new port identifier.
      *aExtraData = mPortIdentifiers.Length();
      mPortIdentifiers.AppendElement(identifier.release());

      *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
      *aContent = nullptr;
      *aOwnership = JS::SCTAG_TMO_CUSTOM;

      return true;
    }

    if (CloneScope() == StructuredCloneScope::SameProcess) {
      OffscreenCanvas* canvas = nullptr;
      rv = UNWRAP_OBJECT(OffscreenCanvas, &obj, canvas);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(canvas);

        UniquePtr<OffscreenCanvasCloneData> clonedCanvas =
            canvas->ToCloneData(aCx);
        if (!clonedCanvas) {
          return false;
        }

        *aExtraData = 0;
        *aTag = SCTAG_DOM_CANVAS;
        *aContent = clonedCanvas.release();
        MOZ_ASSERT(*aContent);
        *aOwnership = JS::SCTAG_TMO_CUSTOM;

        return true;
      }

      ImageBitmap* bitmap = nullptr;
      rv = UNWRAP_OBJECT(ImageBitmap, &obj, bitmap);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(bitmap);
        MOZ_ASSERT(!bitmap->IsWriteOnly());

        *aExtraData = 0;
        *aTag = SCTAG_DOM_IMAGEBITMAP;

        UniquePtr<ImageBitmapCloneData> clonedBitmap = bitmap->ToCloneData();
        if (!clonedBitmap) {
          return false;
        }

        *aContent = clonedBitmap.release();
        MOZ_ASSERT(*aContent);
        *aOwnership = JS::SCTAG_TMO_CUSTOM;

        bitmap->Close();

        return true;
      }

      if (StaticPrefs::dom_media_webcodecs_enabled()) {
        VideoFrame* videoFrame = nullptr;
        rv = UNWRAP_OBJECT(VideoFrame, &obj, videoFrame);
        if (NS_SUCCEEDED(rv)) {
          MOZ_ASSERT(videoFrame);

          *aExtraData = 0;
          *aTag = SCTAG_DOM_VIDEOFRAME;
          *aContent = nullptr;

          UniquePtr<VideoFrame::TransferredData> data = videoFrame->Transfer();
          if (!data) {
            return false;
          }
          *aContent = data.release();
          MOZ_ASSERT(*aContent);
          *aOwnership = JS::SCTAG_TMO_CUSTOM;
          return true;
        }
      }
    }

    {
      RefPtr<ReadableStream> stream;
      rv = UNWRAP_OBJECT(ReadableStream, &obj, stream);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(stream);

        *aTag = SCTAG_DOM_READABLESTREAM;
        *aContent = nullptr;

        UniqueMessagePortId id;
        if (!stream->Transfer(aCx, id)) {
          return false;
        }
        *aExtraData = mPortIdentifiers.Length();
        mPortIdentifiers.AppendElement(id.release());
        *aOwnership = JS::SCTAG_TMO_CUSTOM;
        return true;
      }
    }

    {
      RefPtr<WritableStream> stream;
      rv = UNWRAP_OBJECT(WritableStream, &obj, stream);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(stream);

        *aTag = SCTAG_DOM_WRITABLESTREAM;
        *aContent = nullptr;

        UniqueMessagePortId id;
        if (!stream->Transfer(aCx, id)) {
          return false;
        }
        *aExtraData = mPortIdentifiers.Length();
        mPortIdentifiers.AppendElement(id.release());
        *aOwnership = JS::SCTAG_TMO_CUSTOM;
        return true;
      }
    }

    {
      RefPtr<TransformStream> stream;
      rv = UNWRAP_OBJECT(TransformStream, &obj, stream);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(stream);

        *aTag = SCTAG_DOM_TRANSFORMSTREAM;
        *aContent = nullptr;

        UniqueMessagePortId id1;
        UniqueMessagePortId id2;
        if (!stream->Transfer(aCx, id1, id2)) {
          return false;
        }
        *aExtraData = mPortIdentifiers.Length();
        mPortIdentifiers.AppendElement(id1.release());
        mPortIdentifiers.AppendElement(id2.release());
        *aOwnership = JS::SCTAG_TMO_CUSTOM;
        return true;
      }
    }
  }

  return false;
}

void StructuredCloneHolder::CustomFreeTransferHandler(
    uint32_t aTag, JS::TransferableOwnership aOwnership, void* aContent,
    uint64_t aExtraData) {
  MOZ_ASSERT(mSupportsTransferring);

  if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MOZ_ASSERT(!aContent);
#ifdef FUZZING
    if (aExtraData >= mPortIdentifiers.Length()) {
      return;
    }
#endif
    MOZ_ASSERT(aExtraData < mPortIdentifiers.Length());
    MessagePort::ForceClose(mPortIdentifiers[aExtraData]);
    return;
  }

  if (aTag == SCTAG_DOM_CANVAS &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    MOZ_ASSERT(aContent);
    OffscreenCanvasCloneData* data =
        static_cast<OffscreenCanvasCloneData*>(aContent);
    delete data;
    return;
  }

  if (aTag == SCTAG_DOM_IMAGEBITMAP &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    MOZ_ASSERT(aContent);
    ImageBitmapCloneData* data = static_cast<ImageBitmapCloneData*>(aContent);
    delete data;
    return;
  }

  if (aTag == SCTAG_DOM_READABLESTREAM || aTag == SCTAG_DOM_WRITABLESTREAM) {
    MOZ_ASSERT(!aContent);
#ifdef FUZZING
    if (aExtraData >= mPortIdentifiers.Length()) {
      return;
    }
#endif
    MOZ_ASSERT(aExtraData < mPortIdentifiers.Length());
    MessagePort::ForceClose(mPortIdentifiers[aExtraData]);
    return;
  }

  if (aTag == SCTAG_DOM_TRANSFORMSTREAM) {
    MOZ_ASSERT(!aContent);
#ifdef FUZZING
    if (aExtraData + 1 >= mPortIdentifiers.Length()) {
      return;
    }
#endif
    MOZ_ASSERT(aExtraData + 1 < mPortIdentifiers.Length());
    MessagePort::ForceClose(mPortIdentifiers[aExtraData]);
    MessagePort::ForceClose(mPortIdentifiers[aExtraData + 1]);
    return;
  }

  if (StaticPrefs::dom_media_webcodecs_enabled() &&
      aTag == SCTAG_DOM_VIDEOFRAME &&
      CloneScope() == StructuredCloneScope::SameProcess) {
    if (aContent) {
      VideoFrame::TransferredData* data =
          static_cast<VideoFrame::TransferredData*>(aContent);
      delete data;
    }
    return;
  }
}

bool StructuredCloneHolder::CustomCanTransferHandler(
    JSContext* aCx, JS::Handle<JSObject*> aObj,
    bool* aSameProcessScopeRequired) {
  if (!mSupportsTransferring) {
    return false;
  }

  JS::Rooted<JSObject*> obj(aCx, aObj);

  {
    MessagePort* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, &obj, port);
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

  {
    OffscreenCanvas* canvas = nullptr;
    nsresult rv = UNWRAP_OBJECT(OffscreenCanvas, &obj, canvas);
    if (NS_SUCCEEDED(rv)) {
      SameProcessScopeRequired(aSameProcessScopeRequired);
      return CloneScope() == StructuredCloneScope::SameProcess;
    }
  }

  {
    ImageBitmap* bitmap = nullptr;
    nsresult rv = UNWRAP_OBJECT(ImageBitmap, &obj, bitmap);
    if (NS_SUCCEEDED(rv)) {
      if (bitmap->IsWriteOnly()) {
        return false;
      }

      SameProcessScopeRequired(aSameProcessScopeRequired);
      return CloneScope() == StructuredCloneScope::SameProcess;
    }
  }

  {
    ReadableStream* stream = nullptr;
    nsresult rv = UNWRAP_OBJECT(ReadableStream, &obj, stream);
    if (NS_SUCCEEDED(rv)) {
      // https://streams.spec.whatwg.org/#ref-for-transfer-steps
      // Step 1: If ! IsReadableStreamLocked(value) is true, throw a
      // "DataCloneError" DOMException.
      return !stream->Locked();
    }
  }

  {
    WritableStream* stream = nullptr;
    nsresult rv = UNWRAP_OBJECT(WritableStream, &obj, stream);
    if (NS_SUCCEEDED(rv)) {
      // https://streams.spec.whatwg.org/#ref-for-transfer-steps①
      // Step 1: If ! IsWritableStreamLocked(value) is true, throw a
      // "DataCloneError" DOMException.
      return !stream->Locked();
    }
  }

  {
    TransformStream* stream = nullptr;
    nsresult rv = UNWRAP_OBJECT(TransformStream, &obj, stream);
    if (NS_SUCCEEDED(rv)) {
      // https://streams.spec.whatwg.org/#ref-for-transfer-steps②
      // Step 3 + 4: If ! Is{Readable,Writable}StreamLocked(value) is true,
      // throw a "DataCloneError" DOMException.
      return !stream->Readable()->Locked() && !stream->Writable()->Locked();
    }
  }

  if (StaticPrefs::dom_media_webcodecs_enabled()) {
    VideoFrame* videoframe = nullptr;
    nsresult rv = UNWRAP_OBJECT(VideoFrame, &obj, videoframe);
    if (NS_SUCCEEDED(rv)) {
      SameProcessScopeRequired(aSameProcessScopeRequired);
      return CloneScope() == StructuredCloneScope::SameProcess;
    }
  }

  return false;
}

bool StructuredCloneHolder::TakeTransferredPortsAsSequence(
    Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts) {
  nsTArray<RefPtr<MessagePort>> ports = TakeTransferredPorts();

  aPorts.Clear();
  for (uint32_t i = 0, len = ports.Length(); i < len; ++i) {
    if (!aPorts.AppendElement(ports[i].forget(), fallible)) {
      return false;
    }
  }

  return true;
}

void StructuredCloneHolder::SameProcessScopeRequired(
    bool* aSameProcessScopeRequired) {
  MOZ_ASSERT(aSameProcessScopeRequired);
  if (mStructuredCloneScope == StructuredCloneScope::UnknownDestination) {
    mStructuredCloneScope = StructuredCloneScope::SameProcess;
    *aSameProcessScopeRequired = true;
  }
}

}  // namespace mozilla::dom
