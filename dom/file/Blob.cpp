/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Blob.h"
#include "EmptyBlobImpl.h"
#include "File.h"
#include "MemoryBlobImpl.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BodyStream.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/HoldDropJSObjects.h"
#include "MultipartBlobImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsIInputStream.h"
#include "nsPIDOMWindow.h"
#include "StreamBlobImpl.h"
#include "StringBlobImpl.h"
#include "js/GCAPI.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(Blob)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Blob)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(Blob)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Blob)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Blob)

void Blob::MakeValidBlobType(nsAString& aType) {
  char16_t* iter = aType.BeginWriting();
  char16_t* end = aType.EndWriting();

  for (; iter != end; ++iter) {
    char16_t c = *iter;
    if (c < 0x20 || c > 0x7E) {
      // Non-ASCII char, bail out.
      aType.Truncate();
      return;
    }

    if (c >= 'A' && c <= 'Z') {
      *iter = c + ('a' - 'A');
    }
  }
}

/* static */
Blob* Blob::Create(nsIGlobalObject* aGlobal, BlobImpl* aImpl) {
  MOZ_ASSERT(aImpl);

  MOZ_ASSERT(aGlobal);
  if (NS_WARN_IF(!aGlobal)) {
    return nullptr;
  }

  return aImpl->IsFile() ? new File(aGlobal, aImpl) : new Blob(aGlobal, aImpl);
}

/* static */
already_AddRefed<Blob> Blob::CreateStringBlob(nsIGlobalObject* aGlobal,
                                              const nsACString& aData,
                                              const nsAString& aContentType) {
  MOZ_ASSERT(aGlobal);
  if (NS_WARN_IF(!aGlobal)) {
    return nullptr;
  }

  RefPtr<BlobImpl> blobImpl = StringBlobImpl::Create(aData, aContentType);
  RefPtr<Blob> blob = Blob::Create(aGlobal, blobImpl);
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */
already_AddRefed<Blob> Blob::CreateMemoryBlob(nsIGlobalObject* aGlobal,
                                              void* aMemoryBuffer,
                                              uint64_t aLength,
                                              const nsAString& aContentType) {
  MOZ_ASSERT(aGlobal);
  if (NS_WARN_IF(!aGlobal)) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(
      aGlobal, new MemoryBlobImpl(aMemoryBuffer, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

Blob::Blob(nsIGlobalObject* aGlobal, BlobImpl* aImpl)
    : mImpl(aImpl), mGlobal(aGlobal) {
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(mGlobal);
}

Blob::~Blob() = default;

bool Blob::IsFile() const { return mImpl->IsFile(); }

const nsTArray<RefPtr<BlobImpl>>* Blob::GetSubBlobImpls() const {
  return mImpl->GetSubBlobImpls();
}

already_AddRefed<File> Blob::ToFile() {
  if (!mImpl->IsFile()) {
    return nullptr;
  }

  RefPtr<File> file;
  if (HasFileInterface()) {
    file = static_cast<File*>(this);
  } else {
    file = new File(mGlobal, mImpl);
  }

  return file.forget();
}

already_AddRefed<File> Blob::ToFile(const nsAString& aName,
                                    ErrorResult& aRv) const {
  AutoTArray<RefPtr<BlobImpl>, 1> blobImpls({mImpl});

  nsAutoString contentType;
  mImpl->GetType(contentType);

  RefPtr<MultipartBlobImpl> impl =
      MultipartBlobImpl::Create(std::move(blobImpls), aName, contentType,
                                mGlobal->CrossOriginIsolated(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<File> file = new File(mGlobal, impl);
  return file.forget();
}

already_AddRefed<Blob> Blob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const {
  RefPtr<BlobImpl> impl =
      mImpl->CreateSlice(aStart, aLength, aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(mGlobal, impl);
  return blob.forget();
}

uint64_t Blob::GetSize(ErrorResult& aRv) { return mImpl->GetSize(aRv); }

void Blob::GetType(nsAString& aType) { mImpl->GetType(aType); }

void Blob::GetBlobImplType(nsAString& aBlobImplType) {
  mImpl->GetBlobImplType(aBlobImplType);
}

already_AddRefed<Blob> Blob::Slice(const Optional<int64_t>& aStart,
                                   const Optional<int64_t>& aEnd,
                                   const Optional<nsAString>& aContentType,
                                   ErrorResult& aRv) {
  nsAutoString contentType;
  if (aContentType.WasPassed()) {
    contentType = aContentType.Value();
  }

  RefPtr<BlobImpl> impl = mImpl->Slice(aStart, aEnd, contentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(mGlobal, impl);
  return blob.forget();
}

size_t Blob::GetAllocationSize() const { return mImpl->GetAllocationSize(); }

// contentTypeWithCharset can be set to the contentType or
// contentType+charset based on what the spec says.
// See: https://fetch.spec.whatwg.org/#concept-bodyinit-extract
nsresult Blob::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                           nsACString& aContentType,
                           nsACString& aCharset) const {
  return mImpl->GetSendInfo(aBody, aContentLength, aContentType, aCharset);
}

JSObject* Blob::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return Blob_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<Blob> Blob::Constructor(
    const GlobalObject& aGlobal, const Optional<Sequence<BlobPart>>& aData,
    const BlobPropertyBag& aBag, ErrorResult& aRv) {
  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl();

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  if (aData.WasPassed()) {
    nsAutoString type(aBag.mType);
    MakeValidBlobType(type);
    impl->InitializeBlob(aData.Value(), type,
                         aBag.mEndings == EndingType::Native,
                         global->CrossOriginIsolated(), aRv);
  } else {
    impl->InitializeBlob(global->CrossOriginIsolated(), aRv);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(!impl->IsFile());

  RefPtr<Blob> blob = Blob::Create(global, impl);
  return blob.forget();
}

int64_t Blob::GetFileId() const { return mImpl->GetFileId(); }

bool Blob::IsMemoryFile() const { return mImpl->IsMemoryFile(); }

void Blob::CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv) const {
  mImpl->CreateInputStream(aStream, aRv);
}

size_t BindingJSObjectMallocBytes(Blob* aBlob) {
  MOZ_ASSERT(aBlob);

  // TODO: The hazard analysis currently can't see that none of the
  // implementations of the GetAllocationSize virtual method call can GC (see
  // bug 1531951).
  JS::AutoSuppressGCAnalysis nogc;

  return aBlob->GetAllocationSize();
}

already_AddRefed<Promise> Blob::Text(ErrorResult& aRv) const {
  return ConsumeBody(BodyConsumer::CONSUME_TEXT, aRv);
}

already_AddRefed<Promise> Blob::ArrayBuffer(ErrorResult& aRv) const {
  return ConsumeBody(BodyConsumer::CONSUME_ARRAYBUFFER, aRv);
}

already_AddRefed<Promise> Blob::ConsumeBody(
    BodyConsumer::ConsumeType aConsumeType, ErrorResult& aRv) const {
  if (NS_WARN_IF(!mGlobal)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIEventTarget> mainThreadEventTarget;
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    mainThreadEventTarget = workerPrivate->MainThreadEventTarget();
  } else {
    mainThreadEventTarget = mGlobal->EventTargetFor(TaskCategory::Other);
  }

  MOZ_ASSERT(mainThreadEventTarget);

  nsCOMPtr<nsIInputStream> inputStream;
  CreateInputStream(getter_AddRefs(inputStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return BodyConsumer::Create(mGlobal, mainThreadEventTarget, inputStream,
                              nullptr, aConsumeType, VoidCString(),
                              VoidString(), VoidCString(), VoidCString(),
                              MutableBlobStorage::eOnlyInMemory, aRv);
}

namespace {

class BlobBodyStreamHolder final : public BodyStreamHolder {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BlobBodyStreamHolder,
                                                         BodyStreamHolder)

  BlobBodyStreamHolder() { mozilla::HoldJSObjects(this); }

  void NullifyStream() override { mozilla::DropJSObjects(this); }

  void MarkAsRead() override {}

  void SetReadableStreamBody(ReadableStream* aBody) override {
    mStream = aBody;
  }
  ReadableStream* GetReadableStreamBody() override { return mStream; }

 private:
  RefPtr<ReadableStream> mStream;

 protected:
  ~BlobBodyStreamHolder() override { NullifyStream(); }
};

NS_IMPL_CYCLE_COLLECTION_CLASS(BlobBodyStreamHolder)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BlobBodyStreamHolder,
                                               BodyStreamHolder)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BlobBodyStreamHolder,
                                                  BodyStreamHolder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BlobBodyStreamHolder,
                                                BodyStreamHolder)
  tmp->NullifyStream();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(BlobBodyStreamHolder, BodyStreamHolder)
NS_IMPL_RELEASE_INHERITED(BlobBodyStreamHolder, BodyStreamHolder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BlobBodyStreamHolder)
NS_INTERFACE_MAP_END_INHERITING(BodyStreamHolder)

}  // anonymous namespace

already_AddRefed<ReadableStream> Blob::Stream(JSContext* aCx,
                                              ErrorResult& aRv) const {
  nsCOMPtr<nsIInputStream> stream;
  CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (NS_WARN_IF(!mGlobal)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<BlobBodyStreamHolder> holder = new BlobBodyStreamHolder();

  BodyStream::Create(aCx, holder, mGlobal, stream, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<ReadableStream> rStream = holder->GetReadableStreamBody();
  return rStream.forget();
}

}  // namespace mozilla::dom
