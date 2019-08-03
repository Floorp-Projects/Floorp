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
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "MultipartBlobImpl.h"
#include "nsIInputStream.h"
#include "nsPIDOMWindow.h"
#include "StreamBlobImpl.h"
#include "StringBlobImpl.h"
#include "js/GCAPI.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(Blob)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Blob)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutable)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(Blob)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
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
Blob* Blob::Create(nsISupports* aParent, BlobImpl* aImpl) {
  MOZ_ASSERT(aImpl);

  return aImpl->IsFile() ? new File(aParent, aImpl) : new Blob(aParent, aImpl);
}

/* static */
already_AddRefed<Blob> Blob::CreateEmptyBlob(nsISupports* aParent,
                                             const nsAString& aContentType) {
  RefPtr<Blob> blob = Blob::Create(aParent, new EmptyBlobImpl(aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */
already_AddRefed<Blob> Blob::CreateStringBlob(nsISupports* aParent,
                                              const nsACString& aData,
                                              const nsAString& aContentType) {
  RefPtr<BlobImpl> blobImpl = StringBlobImpl::Create(aData, aContentType);
  RefPtr<Blob> blob = Blob::Create(aParent, blobImpl);
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */
already_AddRefed<Blob> Blob::CreateMemoryBlob(nsISupports* aParent,
                                              void* aMemoryBuffer,
                                              uint64_t aLength,
                                              const nsAString& aContentType) {
  RefPtr<Blob> blob = Blob::Create(
      aParent, new MemoryBlobImpl(aMemoryBuffer, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

Blob::Blob(nsISupports* aParent, BlobImpl* aImpl)
    : mImpl(aImpl), mParent(aParent) {
  MOZ_ASSERT(mImpl);
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
    file = new File(mParent, mImpl);
  }

  return file.forget();
}

already_AddRefed<File> Blob::ToFile(const nsAString& aName,
                                    ErrorResult& aRv) const {
  AutoTArray<RefPtr<BlobImpl>, 1> blobImpls({mImpl});

  nsAutoString contentType;
  mImpl->GetType(contentType);

  RefPtr<MultipartBlobImpl> impl =
      MultipartBlobImpl::Create(std::move(blobImpls), aName, contentType, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<File> file = new File(mParent, impl);
  return file.forget();
}

already_AddRefed<Blob> Blob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) {
  RefPtr<BlobImpl> impl =
      mImpl->CreateSlice(aStart, aLength, aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(mParent, impl);
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

  RefPtr<Blob> blob = Blob::Create(mParent, impl);
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

NS_IMETHODIMP
Blob::GetMutable(bool* aMutable) { return mImpl->GetMutable(aMutable); }

NS_IMETHODIMP
Blob::SetMutable(bool aMutable) { return mImpl->SetMutable(aMutable); }

JSObject* Blob::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return Blob_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<Blob> Blob::Constructor(
    const GlobalObject& aGlobal, const Optional<Sequence<BlobPart>>& aData,
    const BlobPropertyBag& aBag, ErrorResult& aRv) {
  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl();

  if (aData.WasPassed()) {
    nsAutoString type(aBag.mType);
    MakeValidBlobType(type);
    impl->InitializeBlob(aData.Value(), type,
                         aBag.mEndings == EndingTypes::Native, aRv);
  } else {
    impl->InitializeBlob(aRv);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_ASSERT(!impl->IsFile());

  RefPtr<Blob> blob = Blob::Create(aGlobal.GetAsSupports(), impl);
  return blob.forget();
}

int64_t Blob::GetFileId() { return mImpl->GetFileId(); }

bool Blob::IsMemoryFile() const { return mImpl->IsMemoryFile(); }

void Blob::CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv) {
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

already_AddRefed<Promise> Blob::Text(ErrorResult& aRv) {
  return ConsumeBody(BodyConsumer::CONSUME_TEXT, aRv);
}

already_AddRefed<Promise> Blob::ArrayBuffer(ErrorResult& aRv) {
  return ConsumeBody(BodyConsumer::CONSUME_ARRAYBUFFER, aRv);
}

already_AddRefed<Promise> Blob::ConsumeBody(
    BodyConsumer::ConsumeType aConsumeType, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIEventTarget> mainThreadEventTarget;
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    mainThreadEventTarget = workerPrivate->MainThreadEventTarget();
  } else {
    mainThreadEventTarget = global->EventTargetFor(TaskCategory::Other);
  }

  MOZ_ASSERT(mainThreadEventTarget);

  nsCOMPtr<nsIInputStream> inputStream;
  CreateInputStream(getter_AddRefs(inputStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return BodyConsumer::Create(global, mainThreadEventTarget, inputStream,
                              nullptr, aConsumeType, VoidCString(),
                              VoidString(), VoidCString(),
                              MutableBlobStorage::eOnlyInMemory, aRv);
}

namespace {

class BlobBodyStreamHolder final : public BodyStreamHolder {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BlobBodyStreamHolder,
                                                         BodyStreamHolder)

  BlobBodyStreamHolder() { mozilla::HoldJSObjects(this); }

  void NullifyStream() override {
    mozilla::DropJSObjects(this);
    mStream = nullptr;
  }

  void MarkAsRead() override {}

  void SetReadableStreamBody(JSObject* aBody) override {
    MOZ_ASSERT(aBody);
    mStream = aBody;
  }

  JSObject* GetReadableStreamBody() override { return mStream; }

  // Public to make trace happy.
  JS::Heap<JSObject*> mStream;

 private:
  ~BlobBodyStreamHolder() = default;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(BlobBodyStreamHolder)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BlobBodyStreamHolder,
                                               BodyStreamHolder)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStream)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BlobBodyStreamHolder,
                                                  BodyStreamHolder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BlobBodyStreamHolder,
                                                BodyStreamHolder)
  tmp->mStream = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(BlobBodyStreamHolder, BodyStreamHolder)
NS_IMPL_RELEASE_INHERITED(BlobBodyStreamHolder, BodyStreamHolder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BlobBodyStreamHolder)
NS_INTERFACE_MAP_END_INHERITING(BodyStreamHolder)

}  // anonymous namespace

void Blob::Stream(JSContext* aCx, JS::MutableHandle<JSObject*> aStream,
                  ErrorResult& aRv) {
  nsCOMPtr<nsIInputStream> stream;
  CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<BlobBodyStreamHolder> holder = new BlobBodyStreamHolder();

  BodyStream::Create(aCx, holder, global, stream, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aStream.set(holder->GetReadableStreamBody());
}

}  // namespace dom
}  // namespace mozilla
