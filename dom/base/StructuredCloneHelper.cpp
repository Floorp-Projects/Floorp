/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneHelper.h"

#include "ImageContainer.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/CryptoKey.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/StructuredClone.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "MultipartBlobImpl.h"
#include "nsFormData.h"
#include "nsIRemoteBlob.h"
#include "nsQueryObject.h"

#ifdef MOZ_NFC
#include "mozilla/dom/MozNDEFRecord.h"
#endif // MOZ_NFC
#ifdef MOZ_WEBRTC
#include "mozilla/dom/RTCCertificate.h"
#include "mozilla/dom/RTCCertificateBinding.h"
#endif

using namespace mozilla::ipc;

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
  return Write(aCx, aValue, JS::UndefinedHandleValue);
}

bool
StructuredCloneHelperInternal::Write(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     JS::Handle<JS::Value> aTransfer)
{
  MOZ_ASSERT(!mBuffer, "Double Write is not allowed");
  MOZ_ASSERT(!mShutdownCalled, "This method cannot be called after Shutdown.");

  mBuffer = new JSAutoStructuredCloneBuffer(&gCallbacks, this);

  if (!mBuffer->write(aCx, aValue, aTransfer, &gCallbacks, this)) {
    mBuffer = nullptr;
    return false;
  }

  return true;
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
                                             TransferringSupport aSupportsTransferring,
                                             ContextSupport aContext)
  : mSupportsCloning(aSupportsCloning == CloningSupported)
  , mSupportsTransferring(aSupportsTransferring == TransferringSupported)
  , mContext(aContext)
  , mParent(nullptr)
#ifdef DEBUG
  , mCreationThread(NS_GetCurrentThread())
#endif
{}

StructuredCloneHelper::~StructuredCloneHelper()
{
  Shutdown();
  MOZ_ASSERT(mTransferredPorts.IsEmpty());
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
  MOZ_ASSERT_IF(mContext == SameProcessSameThread,
                mCreationThread == NS_GetCurrentThread());

  if (!StructuredCloneHelperInternal::Write(aCx, aValue, aTransfer)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (mContext != SameProcessSameThread) {
    for (uint32_t i = 0, len = mBlobImplArray.Length(); i < len; ++i) {
      if (!mBlobImplArray[i]->MayBeClonedToOtherThreads()) {
        aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
        return;
      }
    }
  }
}

void
StructuredCloneHelper::Read(nsISupports* aParent,
                            JSContext* aCx,
                            JS::MutableHandle<JS::Value> aValue,
                            ErrorResult& aRv)
{
  MOZ_ASSERT_IF(mContext == SameProcessSameThread,
                mCreationThread == NS_GetCurrentThread());

  mozilla::AutoRestore<nsISupports*> guard(mParent);
  mParent = aParent;

  if (!StructuredCloneHelperInternal::Read(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
  }

  // If we are tranferring something, we cannot call 'Read()' more than once.
  if (mSupportsTransferring) {
    mBlobImplArray.Clear();
    mClonedImages.Clear();
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
  MOZ_ASSERT_IF(mContext == SameProcessSameThread,
                mCreationThread == NS_GetCurrentThread());

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
  MOZ_ASSERT_IF(mContext == SameProcessSameThread,
                mCreationThread == NS_GetCurrentThread());

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

/* static */ JSObject*
StructuredCloneHelper::ReadFullySerializableObjects(JSContext* aCx,
                                                    JSStructuredCloneReader* aReader,
                                                    uint32_t aTag,
                                                    uint32_t aIndex)
{
  if (aTag == SCTAG_DOM_IMAGEDATA) {
    return ReadStructuredCloneImageData(aCx, aReader);
  }

  if (aTag == SCTAG_DOM_WEBCRYPTO_KEY) {
    if (!NS_IsMainThread()) {
      return nullptr;
    }

    nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      return nullptr;
    }

    // Prevent the return value from being trashed by a GC during ~nsRefPtr.
    JS::Rooted<JSObject*> result(aCx);
    {
      nsRefPtr<CryptoKey> key = new CryptoKey(global);
      if (!key->ReadStructuredClone(aReader)) {
        result = nullptr;
      } else {
        result = key->WrapObject(aCx, nullptr);
      }
    }
    return result;
  }

  if (aTag == SCTAG_DOM_NULL_PRINCIPAL ||
      aTag == SCTAG_DOM_SYSTEM_PRINCIPAL ||
      aTag == SCTAG_DOM_CONTENT_PRINCIPAL) {
    if (!NS_IsMainThread()) {
      return nullptr;
    }

    mozilla::ipc::PrincipalInfo info;
    if (aTag == SCTAG_DOM_SYSTEM_PRINCIPAL) {
      info = mozilla::ipc::SystemPrincipalInfo();
    } else if (aTag == SCTAG_DOM_NULL_PRINCIPAL) {
      info = mozilla::ipc::NullPrincipalInfo();
    } else {
      uint32_t appId = aIndex;

      uint32_t isInBrowserElement, specLength;
      if (!JS_ReadUint32Pair(aReader, &isInBrowserElement, &specLength)) {
        return nullptr;
      }

      nsAutoCString spec;
      spec.SetLength(specLength);
      if (!JS_ReadBytes(aReader, spec.BeginWriting(), specLength)) {
        return nullptr;
      }

      info = mozilla::ipc::ContentPrincipalInfo(appId, isInBrowserElement,
                                                spec);
    }

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(info, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return nullptr;
    }

    JS::RootedValue result(aCx);
    rv = nsContentUtils::WrapNative(aCx, principal, &NS_GET_IID(nsIPrincipal),
                                    &result);
    if (NS_FAILED(rv)) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return nullptr;
    }

    return result.toObjectOrNull();
  }

#ifdef MOZ_NFC
  if (aTag == SCTAG_DOM_NFC_NDEF) {
    if (!NS_IsMainThread()) {
      return nullptr;
    }

    nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      return nullptr;
    }

    // Prevent the return value from being trashed by a GC during ~nsRefPtr.
    JS::Rooted<JSObject*> result(aCx);
    {
      nsRefPtr<MozNDEFRecord> ndefRecord = new MozNDEFRecord(global);
      result = ndefRecord->ReadStructuredClone(aCx, aReader) ?
               ndefRecord->WrapObject(aCx, nullptr) : nullptr;
    }
    return result;
  }
#endif

#ifdef MOZ_WEBRTC
  if (aTag == SCTAG_DOM_RTC_CERTIFICATE) {
    if (!NS_IsMainThread()) {
      return nullptr;
    }

    nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      return nullptr;
    }

    // Prevent the return value from being trashed by a GC during ~nsRefPtr.
    JS::Rooted<JSObject*> result(aCx);
    {
      nsRefPtr<RTCCertificate> cert = new RTCCertificate(global);
      if (!cert->ReadStructuredClone(aReader)) {
        result = nullptr;
      } else {
        result = cert->WrapObject(aCx, nullptr);
      }
    }
    return result;
  }
#endif

  // Don't know what this is. Bail.
  xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nullptr;
}

/* static */ bool
StructuredCloneHelper::WriteFullySerializableObjects(JSContext* aCx,
                                                     JSStructuredCloneWriter* aWriter,
                                                     JS::Handle<JSObject*> aObj)
{
  // See if this is a ImageData object.
  {
    ImageData* imageData = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageData, aObj, imageData))) {
      return WriteStructuredCloneImageData(aCx, aWriter, imageData);
    }
  }

  // Handle Key cloning
  {
    CryptoKey* key;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(CryptoKey, aObj, key))) {
      MOZ_ASSERT(NS_IsMainThread());
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_WEBCRYPTO_KEY, 0) &&
             key->WriteStructuredClone(aWriter);
    }
  }

#ifdef MOZ_WEBRTC
  {
    // Handle WebRTC Certificate cloning
    RTCCertificate* cert;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(RTCCertificate, aObj, cert))) {
      MOZ_ASSERT(NS_IsMainThread());
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_RTC_CERTIFICATE, 0) &&
             cert->WriteStructuredClone(aWriter);
    }
  }
#endif

  if (NS_IsMainThread() && xpc::IsReflector(aObj)) {
    nsCOMPtr<nsISupports> base = xpc::UnwrapReflectorToISupports(aObj);
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(base);
    if (principal) {
      mozilla::ipc::PrincipalInfo info;
      if (NS_WARN_IF(NS_FAILED(PrincipalToPrincipalInfo(principal, &info)))) {
        xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
        return false;
      }

      if (info.type() == mozilla::ipc::PrincipalInfo::TNullPrincipalInfo) {
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NULL_PRINCIPAL, 0);
      }
      if (info.type() == mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo) {
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_SYSTEM_PRINCIPAL, 0);
      }

      MOZ_ASSERT(info.type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
      const mozilla::ipc::ContentPrincipalInfo& cInfo = info;
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_CONTENT_PRINCIPAL,
                                cInfo.appId()) &&
             JS_WriteUint32Pair(aWriter, cInfo.isInBrowserElement(),
                                cInfo.spec().Length()) &&
             JS_WriteBytes(aWriter, cInfo.spec().get(), cInfo.spec().Length());
    }
  }

#ifdef MOZ_NFC
  {
    MozNDEFRecord* ndefRecord;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(MozNDEFRecord, aObj, ndefRecord))) {
      MOZ_ASSERT(NS_IsMainThread());
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_NFC_NDEF, 0) &&
             ndefRecord->WriteStructuredClone(aCx, aWriter);
    }
  }
#endif // MOZ_NFC

  // Don't know what this is
  xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return false;
}

namespace {

// Recursive!
already_AddRefed<BlobImpl>
EnsureBlobForBackgroundManager(BlobImpl* aBlobImpl,
                               PBackgroundChild* aManager = nullptr)
{
  MOZ_ASSERT(aBlobImpl);

  if (!aManager) {
    aManager = BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(aManager);
  }

  nsRefPtr<BlobImpl> blobImpl = aBlobImpl;

  const nsTArray<nsRefPtr<BlobImpl>>* subBlobImpls =
    aBlobImpl->GetSubBlobImpls();

  if (!subBlobImpls || !subBlobImpls->Length()) {
    if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryObject(blobImpl)) {
      // Always make sure we have a blob from an actor we can use on this
      // thread.
      BlobChild* blobChild = BlobChild::GetOrCreate(aManager, blobImpl);
      MOZ_ASSERT(blobChild);

      blobImpl = blobChild->GetBlobImpl();
      MOZ_ASSERT(blobImpl);

      DebugOnly<bool> isMutable;
      MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
      MOZ_ASSERT(!isMutable);
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));
    }

    return blobImpl.forget();
  }

  const uint32_t subBlobCount = subBlobImpls->Length();
  MOZ_ASSERT(subBlobCount);

  nsTArray<nsRefPtr<BlobImpl>> newSubBlobImpls;
  newSubBlobImpls.SetLength(subBlobCount);

  bool newBlobImplNeeded = false;

  for (uint32_t index = 0; index < subBlobCount; index++) {
    const nsRefPtr<BlobImpl>& subBlobImpl = subBlobImpls->ElementAt(index);
    MOZ_ASSERT(subBlobImpl);

    nsRefPtr<BlobImpl>& newSubBlobImpl = newSubBlobImpls[index];

    newSubBlobImpl = EnsureBlobForBackgroundManager(subBlobImpl, aManager);
    MOZ_ASSERT(newSubBlobImpl);

    if (subBlobImpl != newSubBlobImpl) {
      newBlobImplNeeded = true;
    }
  }

  if (newBlobImplNeeded) {
    nsString contentType;
    blobImpl->GetType(contentType);

    if (blobImpl->IsFile()) {
      nsString name;
      blobImpl->GetName(name);

      blobImpl = new MultipartBlobImpl(newSubBlobImpls, name, contentType);
    } else {
      blobImpl = new MultipartBlobImpl(newSubBlobImpls, contentType);
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));
  }

  return blobImpl.forget();
}

JSObject*
ReadBlob(JSContext* aCx,
         uint32_t aIndex,
         StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(aIndex < aHelper->BlobImpls().Length());
  nsRefPtr<BlobImpl> blobImpl = aHelper->BlobImpls()[aIndex];

  blobImpl = EnsureBlobForBackgroundManager(blobImpl);
  MOZ_ASSERT(blobImpl);

  // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
  // called because the static analysis thinks dereferencing XPCOM objects
  // can GC (because in some cases it can!), and a return statement with a
  // JSObject* type means that JSObject* is on the stack as a raw pointer
  // while destructors are running.
  JS::Rooted<JS::Value> val(aCx);
  {
    nsRefPtr<Blob> blob = Blob::Create(aHelper->ParentDuringRead(), blobImpl);
    if (!ToJSValue(aCx, blob, &val)) {
      return nullptr;
    }
  }

  return &val.toObject();
}

bool
WriteBlob(JSStructuredCloneWriter* aWriter,
          Blob* aBlob,
          StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(aHelper);

  nsRefPtr<BlobImpl> blobImpl = EnsureBlobForBackgroundManager(aBlob->Impl());
  MOZ_ASSERT(blobImpl);

  // We store the position of the blobImpl in the array as index.
  if (JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                         aHelper->BlobImpls().Length())) {
    aHelper->BlobImpls().AppendElement(blobImpl);
    return true;
  }

  return false;
}

// Read the WriteFileList for the format.
JSObject*
ReadFileList(JSContext* aCx,
             JSStructuredCloneReader* aReader,
             uint32_t aCount,
             StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);

  JS::Rooted<JS::Value> val(aCx);
  {
    nsRefPtr<FileList> fileList = new FileList(aHelper->ParentDuringRead());

    uint32_t tag, offset;
    // Offset is the index of the blobImpl from which we can find the blobImpl
    // for this FileList.
    if (!JS_ReadUint32Pair(aReader, &tag, &offset)) {
      return nullptr;
    }

    MOZ_ASSERT(tag == 0);

    // |aCount| is the number of BlobImpls to use from the |offset|.
    for (uint32_t i = 0; i < aCount; ++i) {
      uint32_t index = offset + i;
      MOZ_ASSERT(index < aHelper->BlobImpls().Length());

      nsRefPtr<BlobImpl> blobImpl = aHelper->BlobImpls()[index];
      MOZ_ASSERT(blobImpl->IsFile());

      blobImpl = EnsureBlobForBackgroundManager(blobImpl);
      MOZ_ASSERT(blobImpl);

      nsRefPtr<File> file = File::Create(aHelper->ParentDuringRead(), blobImpl);
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
bool
WriteFileList(JSStructuredCloneWriter* aWriter,
              FileList* aFileList,
              StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aFileList);
  MOZ_ASSERT(aHelper);

  // A FileList is serialized writing the X number of elements and the offset
  // from mBlobImplArray. The Read will take X elements from mBlobImplArray
  // starting from the offset.
  if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_FILELIST,
                          aFileList->Length()) ||
      !JS_WriteUint32Pair(aWriter, 0,
                          aHelper->BlobImpls().Length())) {
    return false;
  }

  for (uint32_t i = 0; i < aFileList->Length(); ++i) {
    nsRefPtr<BlobImpl> blobImpl =
      EnsureBlobForBackgroundManager(aFileList->Item(i)->Impl());
    MOZ_ASSERT(blobImpl);

    aHelper->BlobImpls().AppendElement(blobImpl);
  }

  return true;
}

// Read the WriteFormData for the format.
JSObject*
ReadFormData(JSContext* aCx,
             JSStructuredCloneReader* aReader,
             uint32_t aCount,
             StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(aHelper);

  // See the serialization of the FormData for the format.
  JS::Rooted<JS::Value> val(aCx);
  {
    nsRefPtr<nsFormData> formData =
      new nsFormData(aHelper->ParentDuringRead());

    Optional<nsAString> thirdArg;
    for (uint32_t i = 0; i < aCount; ++i) {
      nsAutoString name;
      if (!ReadString(aReader, name)) {
        return nullptr;
      }

      uint32_t tag, indexOrLengthOfString;
      if (!JS_ReadUint32Pair(aReader, &tag, &indexOrLengthOfString)) {
        return nullptr;
      }

      if (tag == SCTAG_DOM_BLOB) {
        MOZ_ASSERT(indexOrLengthOfString < aHelper->BlobImpls().Length());

        nsRefPtr<BlobImpl> blobImpl =
          aHelper->BlobImpls()[indexOrLengthOfString];
        MOZ_ASSERT(blobImpl->IsFile());

        nsRefPtr<File> file =
          File::Create(aHelper->ParentDuringRead(), blobImpl);
        MOZ_ASSERT(file);

        formData->Append(name, *file, thirdArg);
      } else {
        MOZ_ASSERT(tag == 0);

        nsAutoString value;
        value.SetLength(indexOrLengthOfString);
        size_t charSize = sizeof(nsString::char_type);
        if (!JS_ReadBytes(aReader, (void*) value.BeginWriting(),
                          indexOrLengthOfString * charSize)) {
          return nullptr;
        }

        formData->Append(name, value);
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
//   - else:
//     - pair of ints: 0, string length
//     - value string
bool
WriteFormData(JSStructuredCloneWriter* aWriter,
              nsFormData* aFormData,
              StructuredCloneHelper* aHelper)
{
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aFormData);
  MOZ_ASSERT(aHelper);

  if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_FORMDATA,
                          aFormData->Length())) {
    return false;
  }

  class MOZ_STACK_CLASS Closure final
  {
    JSStructuredCloneWriter* mWriter;
    StructuredCloneHelper* mHelper;

  public:
    Closure(JSStructuredCloneWriter* aWriter,
            StructuredCloneHelper* aHelper)
      : mWriter(aWriter),
        mHelper(aHelper)
    { }

    static bool
    Write(const nsString& aName, bool isFile, const nsString& aValue,
          File* aFile, void* aClosure)
    {
      Closure* closure = static_cast<Closure*>(aClosure);
      if (!WriteString(closure->mWriter, aName)) {
        return false;
      }

      if (isFile) {
        BlobImpl* blobImpl = aFile->Impl();
        if (!JS_WriteUint32Pair(closure->mWriter, SCTAG_DOM_BLOB,
                                closure->mHelper->BlobImpls().Length())) {
          return false;
        }

        closure->mHelper->BlobImpls().AppendElement(blobImpl);
        return true;
      }

      size_t charSize = sizeof(nsString::char_type);
      if (!JS_WriteUint32Pair(closure->mWriter, 0, aValue.Length()) ||
          !JS_WriteBytes(closure->mWriter, aValue.get(),
                         aValue.Length() * charSize)) {
        return false;
      }

      return true;
    }
  };
  Closure closure(aWriter, aHelper);
  return aFormData->ForEach(Closure::Write, &closure);
}

} // anonymous namespace

JSObject*
StructuredCloneHelper::ReadCallback(JSContext* aCx,
                                    JSStructuredCloneReader* aReader,
                                    uint32_t aTag,
                                    uint32_t aIndex)
{
  MOZ_ASSERT(mSupportsCloning);

  if (aTag == SCTAG_DOM_BLOB) {
    return ReadBlob(aCx, aIndex, this);
  }

  if (aTag == SCTAG_DOM_FILELIST) {
    return ReadFileList(aCx, aReader, aIndex, this);
  }

  if (aTag == SCTAG_DOM_FORMDATA) {
    return ReadFormData(aCx, aReader, aIndex, this);
  }

  if (aTag == SCTAG_DOM_IMAGEBITMAP) {
    MOZ_ASSERT(mContext == SameProcessSameThread ||
               mContext == SameProcessDifferentThread);

    // Get the current global object.
    // This can be null.
    nsCOMPtr<nsIGlobalObject> parent = do_QueryInterface(mParent);
    // aIndex is the index of the cloned image.
    return ImageBitmap::ReadStructuredClone(aCx, aReader,
                                            parent, GetImages(), aIndex);
   }

  return ReadFullySerializableObjects(aCx, aReader, aTag, aIndex);
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
      return WriteBlob(aWriter, blob, this);
    }
  }

  // See if this is a FileList object.
  {
    FileList* fileList = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FileList, aObj, fileList))) {
      return WriteFileList(aWriter, fileList, this);
    }
  }

  // See if this is a FormData object.
  {
    nsFormData* formData = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(FormData, aObj, formData))) {
      return WriteFormData(aWriter, formData, this);
    }
  }

  // See if this is an ImageBitmap object.
  if (mContext == SameProcessSameThread ||
      mContext == SameProcessDifferentThread) {
    ImageBitmap* imageBitmap = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageBitmap, aObj, imageBitmap))) {
      return ImageBitmap::WriteStructuredClone(aWriter,
                                               GetImages(),
                                               imageBitmap);
    }
  }

  return WriteFullySerializableObjects(aCx, aWriter, aObj);
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
    MessagePort* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
    if (NS_SUCCEEDED(rv)) {
      // We use aExtraData to store the index of this new port identifier.
      *aExtraData = mPortIdentifiers.Length();
      MessagePortIdentifier* identifier = mPortIdentifiers.AppendElement();

      port->CloneAndDisentangle(*identifier);

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
