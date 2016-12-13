/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/File.h"

#include "ipc/nsIRemoteBlob.h"
#include "MultipartBlobImpl.h"
#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsICharsetDetector.h"
#include "nsIConverterInputStream.h"
#include "nsIDocument.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMIMEService.h"
#include "nsISeekableStream.h"
#include "nsIUnicharInputStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIUUIDGenerator.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsStringStream.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/SHA1.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsThreadUtils.h"
#include "nsStreamUtils.h"
#include "SlicedInputStream.h"

namespace mozilla {
namespace dom {

using namespace workers;

// XXXkhuey the input stream that we pass out of a File
// can outlive the actual File object.  Thus, we must
// ensure that the buffer underlying the stream we get
// from NS_NewByteInputStream is held alive as long as the
// stream is.  We do that by passing back this class instead.
class DataOwnerAdapter final : public nsIInputStream,
                               public nsISeekableStream,
                               public nsIIPCSerializableInputStream
{
  typedef BlobImplMemory::DataOwner DataOwner;
public:
  static nsresult Create(DataOwner* aDataOwner,
                         uint32_t aStart,
                         uint32_t aLength,
                         nsIInputStream** _retval);

  NS_DECL_THREADSAFE_ISUPPORTS

  // These are mandatory.
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_NSISEEKABLESTREAM(mSeekableStream->)

  // This is optional. We use a conditional QI to keep it from being called
  // if the underlying stream doesn't support it.
  NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(mSerializableInputStream->)

private:
  ~DataOwnerAdapter() {}

  DataOwnerAdapter(DataOwner* aDataOwner,
                   nsIInputStream* aStream)
    : mDataOwner(aDataOwner), mStream(aStream),
      mSeekableStream(do_QueryInterface(aStream)),
      mSerializableInputStream(do_QueryInterface(aStream))
  {
    MOZ_ASSERT(mSeekableStream, "Somebody gave us the wrong stream!");
  }

  RefPtr<DataOwner> mDataOwner;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializableInputStream;
};

NS_IMPL_ADDREF(DataOwnerAdapter)
NS_IMPL_RELEASE(DataOwnerAdapter)

NS_INTERFACE_MAP_BEGIN(DataOwnerAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

nsresult DataOwnerAdapter::Create(DataOwner* aDataOwner,
                                  uint32_t aStart,
                                  uint32_t aLength,
                                  nsIInputStream** _retval)
{
  nsresult rv;
  MOZ_ASSERT(aDataOwner, "Uh ...");

  nsCOMPtr<nsIInputStream> stream;

  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             static_cast<const char*>(aDataOwner->mData) +
                             aStart,
                             (int32_t)aLength,
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = new DataOwnerAdapter(aDataOwner, stream));

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::Blob implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(Blob)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Blob)
  // This class should not receive any nsIRemoteBlob QI!
  MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsIRemoteBlob)));

  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Blob)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Blob)

// A utility function that enforces the spec constraints on the type of a blob:
// no codepoints outside the ASCII range (otherwise type becomes empty) and
// lowercase ASCII only.  We can't just use our existing nsContentUtils
// ASCII-related helpers because we need the "outside ASCII range" check, and we
// can't use NS_IsAscii because its definition of "ASCII" (chars all <= 0x7E)
// differs from the file API definition (which excludes control chars).
static void
MakeValidBlobType(nsAString& aType)
{
  char16_t* iter = aType.BeginWriting();
  char16_t* end = aType.EndWriting();

  for ( ; iter != end; ++iter) {
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

/* static */ Blob*
Blob::Create(nsISupports* aParent, BlobImpl* aImpl)
{
  MOZ_ASSERT(aImpl);

  return aImpl->IsFile() ? new File(aParent, aImpl)
                         : new Blob(aParent, aImpl);
}

/* static */ already_AddRefed<Blob>
Blob::CreateStringBlob(nsISupports* aParent, const nsACString& aData,
                       const nsAString& aContentType)
{
  RefPtr<BlobImpl> blobImpl = BlobImplString::Create(aData, aContentType);
  RefPtr<Blob> blob = Blob::Create(aParent, blobImpl);
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::CreateMemoryBlob(nsISupports* aParent, void* aMemoryBuffer,
                       uint64_t aLength, const nsAString& aContentType)
{
  RefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplMemory(aMemoryBuffer, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::CreateTemporaryBlob(nsISupports* aParent, PRFileDesc* aFD,
                          uint64_t aStartPos, uint64_t aLength,
                          const nsAString& aContentType)
{
  RefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplTemporaryBlob(aFD, aStartPos, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

Blob::Blob(nsISupports* aParent, BlobImpl* aImpl)
  : mImpl(aImpl)
  , mParent(aParent)
{
  MOZ_ASSERT(mImpl);

#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aParent);
    if (win) {
      MOZ_ASSERT(win->IsInnerWindow());
    }
  }
#endif
}

bool
Blob::IsFile() const
{
  return mImpl->IsFile();
}

const nsTArray<RefPtr<BlobImpl>>*
Blob::GetSubBlobImpls() const
{
  return mImpl->GetSubBlobImpls();
}

already_AddRefed<File>
Blob::ToFile()
{
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

already_AddRefed<File>
Blob::ToFile(const nsAString& aName, ErrorResult& aRv) const
{
  AutoTArray<RefPtr<BlobImpl>, 1> blobImpls({mImpl});

  nsAutoString contentType;
  mImpl->GetType(contentType);

  RefPtr<MultipartBlobImpl> impl =
    MultipartBlobImpl::Create(Move(blobImpls), aName, contentType, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<File> file = new File(mParent, impl);
  return file.forget();
}

already_AddRefed<Blob>
Blob::CreateSlice(uint64_t aStart, uint64_t aLength,
                  const nsAString& aContentType,
                  ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl = mImpl->CreateSlice(aStart, aLength,
                                             aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(mParent, impl);
  return blob.forget();
}

uint64_t
Blob::GetSize(ErrorResult& aRv)
{
  return mImpl->GetSize(aRv);
}

void
Blob::GetType(nsAString &aType)
{
  mImpl->GetType(aType);
}

already_AddRefed<Blob>
Blob::Slice(const Optional<int64_t>& aStart,
            const Optional<int64_t>& aEnd,
            const nsAString& aContentType,
            ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    mImpl->Slice(aStart, aEnd, aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(mParent, impl);
  return blob.forget();
}

NS_IMETHODIMP
Blob::GetSendInfo(nsIInputStream** aBody,
                  uint64_t* aContentLength,
                  nsACString& aContentType,
                  nsACString& aCharset)
{
  return mImpl->GetSendInfo(aBody, aContentLength, aContentType, aCharset);
}

NS_IMETHODIMP
Blob::GetMutable(bool* aMutable)
{
  return mImpl->GetMutable(aMutable);
}

NS_IMETHODIMP
Blob::SetMutable(bool aMutable)
{
  return mImpl->SetMutable(aMutable);
}

JSObject*
Blob::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BlobBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<Blob>
Blob::Constructor(const GlobalObject& aGlobal,
                  const Optional<Sequence<BlobPart>>& aData,
                  const BlobPropertyBag& aBag,
                  ErrorResult& aRv)
{
  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl();

  if (aData.WasPassed()) {
    nsAutoString type(aBag.mType);
    MakeValidBlobType(type);
    impl->InitializeBlob(aGlobal.Context(), aData.Value(), type,
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

int64_t
Blob::GetFileId()
{
  return mImpl->GetFileId();
}

bool
Blob::IsMemoryFile() const
{
  return mImpl->IsMemoryFile();
}

void
Blob::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  mImpl->GetInternalStream(aStream, aRv);
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::File implementation

File::File(nsISupports* aParent, BlobImpl* aImpl)
  : Blob(aParent, aImpl)
{
  MOZ_ASSERT(aImpl->IsFile());
}

/* static */ File*
File::Create(nsISupports* aParent, BlobImpl* aImpl)
{
  MOZ_ASSERT(aImpl);
  MOZ_ASSERT(aImpl->IsFile());

  return new File(aParent, aImpl);
}

/* static */ already_AddRefed<File>
File::Create(nsISupports* aParent, const nsAString& aName,
             const nsAString& aContentType, uint64_t aLength,
             int64_t aLastModifiedDate)
{
  RefPtr<File> file = new File(aParent,
    new BlobImplBase(aName, aContentType, aLength, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateMemoryFile(nsISupports* aParent, void* aMemoryBuffer,
                       uint64_t aLength, const nsAString& aName,
                       const nsAString& aContentType,
                       int64_t aLastModifiedDate)
{
  RefPtr<File> file = new File(aParent,
    new BlobImplMemory(aMemoryBuffer, aLength, aName,
                       aContentType, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, nsIFile* aFile, bool aTemporary)
{
  RefPtr<File> file = new File(aParent, new BlobImplFile(aFile, aTemporary));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                     const nsAString& aName, const nsAString& aContentType)
{
  RefPtr<File> file = new File(aParent,
    new BlobImplFile(aFile, aName, aContentType));
  return file.forget();
}

JSObject*
File::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileBinding::Wrap(aCx, this, aGivenProto);
}

void
File::GetName(nsAString& aFileName) const
{
  mImpl->GetName(aFileName);
}

void
File::GetPath(nsAString& aPath) const
{
  mImpl->GetPath(aPath);
}

void
File::SetPath(const nsAString& aPath)
{
  mImpl->SetPath(aPath);
}

Date
File::GetLastModifiedDate(ErrorResult& aRv)
{
  int64_t value = GetLastModified(aRv);
  if (aRv.Failed()) {
    return Date();
  }

  return Date(JS::TimeClip(value));
}

int64_t
File::GetLastModified(ErrorResult& aRv)
{
  return mImpl->GetLastModified(aRv);
}

void
File::GetMozFullPath(nsAString& aFilename, ErrorResult& aRv) const
{
  mImpl->GetMozFullPath(aFilename, aRv);
}

void
File::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const
{
  mImpl->GetMozFullPathInternal(aFileName, aRv);
}

// Makes sure that aStart and aEnd is less then or equal to aSize and greater
// than 0
static void
ParseSize(int64_t aSize, int64_t& aStart, int64_t& aEnd)
{
  CheckedInt64 newStartOffset = aStart;
  if (aStart < -aSize) {
    newStartOffset = 0;
  }
  else if (aStart < 0) {
    newStartOffset += aSize;
  }
  else if (aStart > aSize) {
    newStartOffset = aSize;
  }

  CheckedInt64 newEndOffset = aEnd;
  if (aEnd < -aSize) {
    newEndOffset = 0;
  }
  else if (aEnd < 0) {
    newEndOffset += aSize;
  }
  else if (aEnd > aSize) {
    newEndOffset = aSize;
  }

  if (!newStartOffset.isValid() || !newEndOffset.isValid() ||
      newStartOffset.value() >= newEndOffset.value()) {
    aStart = aEnd = 0;
  }
  else {
    aStart = newStartOffset.value();
    aEnd = newEndOffset.value();
  }
}

/* static */ already_AddRefed<File>
File::Constructor(const GlobalObject& aGlobal,
                  const Sequence<BlobPart>& aData,
                  const nsAString& aName,
                  const FilePropertyBag& aBag,
                  ErrorResult& aRv)
{
  // Normalizing the filename
  nsString name(aName);
  name.ReplaceChar('/', ':');

  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(name);

  nsAutoString type(aBag.mType);
  MakeValidBlobType(type);
  impl->InitializeBlob(aGlobal.Context(), aData, type, false, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  RefPtr<File> file = new File(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromNsIFile(const GlobalObject& aGlobal,
                        nsIFile* aData,
                        const ChromeFilePropertyBag& aBag,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  RefPtr<File> domFile = new File(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFileName(const GlobalObject& aGlobal,
                         const nsAString& aData,
                         const ChromeFilePropertyBag& aBag,
                         ErrorResult& aRv)
{
  if (!nsContentUtils::ThreadsafeIsCallerChrome()) {
    aRv.ThrowTypeError<MSG_MISSING_ARGUMENTS>(NS_LITERAL_STRING("File"));
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  RefPtr<File> domFile = new File(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::BlobImpl implementation

already_AddRefed<BlobImpl>
BlobImpl::Slice(const Optional<int64_t>& aStart,
                const Optional<int64_t>& aEnd,
                const nsAString& aContentType,
                ErrorResult& aRv)
{
  // Truncate aStart and aEnd so that we stay within this file.
  uint64_t thisLength = GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  int64_t start = aStart.WasPassed() ? aStart.Value() : 0;
  int64_t end = aEnd.WasPassed() ? aEnd.Value() : (int64_t)thisLength;

  ParseSize((int64_t)thisLength, start, end);

  nsAutoString type(aContentType);
  MakeValidBlobType(type);
  return CreateSlice((uint64_t)start, (uint64_t)(end - start), type, aRv);
}

////////////////////////////////////////////////////////////////////////////
// BlobImpl implementation

NS_IMPL_ISUPPORTS(BlobImpl, BlobImpl)

////////////////////////////////////////////////////////////////////////////
// BlobImplFile implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplFile, BlobImpl)

void
BlobImplBase::GetName(nsAString& aName) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aName = mName;
}

void
BlobImplBase::GetPath(nsAString& aPath) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aPath = mPath;
}

void
BlobImplBase::SetPath(const nsAString& aPath)
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  mPath = aPath;
}

void
BlobImplBase::GetMozFullPath(nsAString& aFileName, ErrorResult& aRv) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");

  aFileName.Truncate();

  if (NS_IsMainThread()) {
    if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
      GetMozFullPathInternal(aFileName, aRv);
    }

    return;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (workerPrivate->UsesSystemPrincipal()) {
    GetMozFullPathInternal(aFileName, aRv);
  }
}

void
BlobImplBase::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const
{
  if (!mIsFile) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFileName.Truncate();
}

void
BlobImplBase::GetType(nsAString& aType)
{
  aType = mContentType;
}

int64_t
BlobImplBase::GetLastModified(ErrorResult& aRv)
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = PR_Now();
  }

  return mLastModificationDate / PR_USEC_PER_MSEC;
}

void
BlobImplBase::SetLastModified(int64_t aLastModified)
{
  mLastModificationDate = aLastModified * PR_USEC_PER_MSEC;
}

int64_t
BlobImplBase::GetFileId()
{
  return -1;
}

nsresult
BlobImplBase::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                          nsACString& aContentType, nsACString& aCharset)
{
  MOZ_ASSERT(aContentLength);

  ErrorResult rv;

  nsCOMPtr<nsIInputStream> stream;
  GetInternalStream(getter_AddRefs(stream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  *aContentLength = GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString contentType;
  GetType(contentType);

  if (contentType.IsEmpty()) {
    aContentType.SetIsVoid(true);
  } else {
    CopyUTF16toUTF8(contentType, aContentType);
  }

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

nsresult
BlobImplBase::GetMutable(bool* aMutable) const
{
  *aMutable = !mImmutable;
  return NS_OK;
}

nsresult
BlobImplBase::SetMutable(bool aMutable)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(!mImmutable || !aMutable);

  if (!mImmutable && !aMutable) {
    // Force the content type and size to be cached
    nsAutoString dummyString;
    GetType(dummyString);

    ErrorResult error;
    GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  mImmutable = !aMutable;
  return rv;
}

/* static */ uint64_t
BlobImplBase::NextSerialNumber()
{
  static Atomic<uint64_t> nextSerialNumber;
  return nextSerialNumber++;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplFile implementation

already_AddRefed<BlobImpl>
BlobImplFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                          const nsAString& aContentType,
                          ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    new BlobImplFile(this, aStart, aLength, aContentType);
  return impl.forget();
}

void
BlobImplFile::GetMozFullPathInternal(nsAString& aFilename, ErrorResult& aRv) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aRv = mFile->GetPath(aFilename);
}

uint64_t
BlobImplFile::GetSize(ErrorResult& aRv)
{
  if (BlobImplBase::IsSizeUnknown()) {
    MOZ_ASSERT(mWholeFile,
                 "Should only use lazy size when using the whole file");
    int64_t fileSize;
    aRv = mFile->GetFileSize(&fileSize);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    if (fileSize < 0) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }

    mLength = fileSize;
  }

  return mLength;
}

namespace {

class GetTypeRunnable final : public WorkerMainThreadRunnable
{
public:
  GetTypeRunnable(WorkerPrivate* aWorkerPrivate,
                  BlobImpl* aBlobImpl)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("BlobImplFile :: GetType"))
    , mBlobImpl(aBlobImpl)
  {
    MOZ_ASSERT(aBlobImpl);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoString type;
    mBlobImpl->GetType(type);
    return true;
  }

private:
  ~GetTypeRunnable()
  {}

  RefPtr<BlobImpl> mBlobImpl;
};

} // anonymous namespace

void
BlobImplFile::GetType(nsAString& aType)
{
  aType.Truncate();

  if (mContentType.IsVoid()) {
    MOZ_ASSERT(mWholeFile,
               "Should only use lazy ContentType when using the whole file");

    if (!NS_IsMainThread()) {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      if (!workerPrivate) {
        // I have no idea in which thread this method is called. We cannot
        // return any valid value.
        return;
      }

      RefPtr<GetTypeRunnable> runnable =
        new GetTypeRunnable(workerPrivate, this);

      ErrorResult rv;
      runnable->Dispatch(rv);
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
      }
      return;
    }

    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsAutoCString mimeType;
    rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      mimeType.Truncate();
    }

    AppendUTF8toUTF16(mimeType, mContentType);
    mContentType.SetIsVoid(false);
  }

  aType = mContentType;
}

int64_t
BlobImplFile::GetLastModified(ErrorResult& aRv)
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  if (BlobImplBase::IsDateUnknown()) {
    PRTime msecs;
    aRv = mFile->GetLastModifiedTime(&msecs);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    mLastModificationDate = msecs;
  }

  return mLastModificationDate;
}

void
BlobImplFile::SetLastModified(int64_t aLastModified)
{
  MOZ_CRASH("SetLastModified of a real file is not allowed!");
}

const uint32_t sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN |
  nsIFileInputStream::SHARE_DELETE;

void
BlobImplFile::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  if (mWholeFile) {
    aRv = NS_NewLocalFileInputStream(aStream, mFile, -1, -1, sFileStreamFlags);
    return;
  }

  aRv = NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                          -1, -1, sFileStreamFlags);
}

bool
BlobImplFile::IsDirectory() const
{
  bool isDirectory = false;
  if (mFile) {
    mFile->IsDirectory(&isDirectory);
  }
  return isDirectory;
}

////////////////////////////////////////////////////////////////////////////
// EmptyBlobImpl implementation

NS_IMPL_ISUPPORTS_INHERITED0(EmptyBlobImpl, BlobImpl)

already_AddRefed<BlobImpl>
EmptyBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                           const nsAString& aContentType,
                           ErrorResult& aRv)
{
  MOZ_ASSERT(!aStart && !aLength);
  RefPtr<BlobImpl> impl = new EmptyBlobImpl(aContentType);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(impl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  return impl.forget();
}

void
EmptyBlobImpl::GetInternalStream(nsIInputStream** aStream,
                                 ErrorResult& aRv)
{
  if (NS_WARN_IF(!aStream)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = NS_NewCStringInputStream(aStream, EmptyCString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////
// BlobImplString implementation

NS_IMPL_ISUPPORTS_INHERITED(BlobImplString, BlobImpl, nsIMemoryReporter)

/* static */ already_AddRefed<BlobImplString>
BlobImplString::Create(const nsACString& aData, const nsAString& aContentType)
{
  RefPtr<BlobImplString> blobImpl = new BlobImplString(aData, aContentType);
  RegisterWeakMemoryReporter(blobImpl);
  return blobImpl.forget();
}

BlobImplString::BlobImplString(const nsACString& aData,
                               const nsAString& aContentType)
  : BlobImplBase(aContentType, aData.Length())
  , mData(aData)
{
}

BlobImplString::~BlobImplString()
{
  UnregisterWeakMemoryReporter(this);
}

already_AddRefed<BlobImpl>
BlobImplString::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    new BlobImplString(Substring(mData, aStart, aLength),
                       aContentType);
  return impl.forget();
}

void
BlobImplString::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  aRv = NS_NewCStringInputStream(aStream, mData);
}

NS_IMETHODIMP
BlobImplString::CollectReports(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/dom/memory-file-data/string", KIND_HEAP, UNITS_BYTES,
    mData.SizeOfExcludingThisIfUnshared(MallocSizeOf),
    "Memory used to back a File/Blob based on a string.");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplMemory implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplMemory, BlobImpl)

already_AddRefed<BlobImpl>
BlobImplMemory::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    new BlobImplMemory(this, aStart, aLength, aContentType);
  return impl.forget();
}

void
BlobImplMemory::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  if (mLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aRv = DataOwnerAdapter::Create(mDataOwner, mStart, mLength, aStream);
}

/* static */ StaticMutex
BlobImplMemory::DataOwner::sDataOwnerMutex;

/* static */ StaticAutoPtr<LinkedList<BlobImplMemory::DataOwner>>
BlobImplMemory::DataOwner::sDataOwners;

/* static */ bool
BlobImplMemory::DataOwner::sMemoryReporterRegistered = false;

MOZ_DEFINE_MALLOC_SIZE_OF(MemoryFileDataOwnerMallocSizeOf)

class BlobImplMemoryDataOwnerMemoryReporter final
  : public nsIMemoryReporter
{
  ~BlobImplMemoryDataOwnerMemoryReporter() {}

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    typedef BlobImplMemory::DataOwner DataOwner;

    StaticMutexAutoLock lock(DataOwner::sDataOwnerMutex);

    if (!DataOwner::sDataOwners) {
      return NS_OK;
    }

    const size_t LARGE_OBJECT_MIN_SIZE = 8 * 1024;
    size_t smallObjectsTotal = 0;

    for (DataOwner *owner = DataOwner::sDataOwners->getFirst();
         owner; owner = owner->getNext()) {

      size_t size = MemoryFileDataOwnerMallocSizeOf(owner->mData);

      if (size < LARGE_OBJECT_MIN_SIZE) {
        smallObjectsTotal += size;
      } else {
        SHA1Sum sha1;
        sha1.update(owner->mData, owner->mLength);
        uint8_t digest[SHA1Sum::kHashSize]; // SHA1 digests are 20 bytes long.
        sha1.finish(digest);

        nsAutoCString digestString;
        for (size_t i = 0; i < sizeof(digest); i++) {
          digestString.AppendPrintf("%02x", digest[i]);
        }

        aHandleReport->Callback(
          /* process */ NS_LITERAL_CSTRING(""),
          nsPrintfCString(
            "explicit/dom/memory-file-data/large/file(length=%llu, sha1=%s)",
            owner->mLength, aAnonymize ? "<anonymized>" : digestString.get()),
          KIND_HEAP, UNITS_BYTES, size,
          nsPrintfCString(
            "Memory used to back a memory file of length %llu bytes.  The file "
            "has a sha1 of %s.\n\n"
            "Note that the allocator may round up a memory file's length -- "
            "that is, an N-byte memory file may take up more than N bytes of "
            "memory.",
            owner->mLength, digestString.get()),
          aData);
      }
    }

    if (smallObjectsTotal > 0) {
      aHandleReport->Callback(
        /* process */ NS_LITERAL_CSTRING(""),
        NS_LITERAL_CSTRING("explicit/dom/memory-file-data/small"),
        KIND_HEAP, UNITS_BYTES, smallObjectsTotal,
        nsPrintfCString(
          "Memory used to back small memory files (i.e. those taking up less "
          "than %zu bytes of memory each).\n\n"
          "Note that the allocator may round up a memory file's length -- "
          "that is, an N-byte memory file may take up more than N bytes of "
          "memory.", LARGE_OBJECT_MIN_SIZE),
        aData);
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(BlobImplMemoryDataOwnerMemoryReporter, nsIMemoryReporter)

/* static */ void
BlobImplMemory::DataOwner::EnsureMemoryReporterRegistered()
{
  sDataOwnerMutex.AssertCurrentThreadOwns();
  if (sMemoryReporterRegistered) {
    return;
  }

  RegisterStrongMemoryReporter(new BlobImplMemoryDataOwnerMemoryReporter());

  sMemoryReporterRegistered = true;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplTemporaryBlob implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplTemporaryBlob, BlobImpl)

already_AddRefed<BlobImpl>
BlobImplTemporaryBlob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                   const nsAString& aContentType,
                                   ErrorResult& aRv)
{
  if (aStart + aLength > mLength) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<BlobImpl> impl =
    new BlobImplTemporaryBlob(this, aStart + mStartPos,
                              aLength, aContentType);
  return impl.forget();
}

void
BlobImplTemporaryBlob::GetInternalStream(nsIInputStream** aStream,
                                         ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream =
    new nsTemporaryFileInputStream(mFileDescOwner, mStartPos, mStartPos + mLength);
  stream.forget(aStream);
}

////////////////////////////////////////////////////////////////////////////
// BlobImplStream implementation

NS_IMPL_ISUPPORTS_INHERITED(BlobImplStream, BlobImpl, nsIMemoryReporter)

/* static */ already_AddRefed<BlobImplStream>
BlobImplStream::Create(nsIInputStream* aInputStream,
                       const nsAString& aContentType,
                       uint64_t aLength)
{
  RefPtr<BlobImplStream> blobImplStream =
    new BlobImplStream(aInputStream, aContentType, aLength);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

/* static */ already_AddRefed<BlobImplStream>
BlobImplStream::Create(nsIInputStream* aInputStream,
                       const nsAString& aName,
                       const nsAString& aContentType,
                       int64_t aLastModifiedDate,
                       uint64_t aLength)
{
  RefPtr<BlobImplStream> blobImplStream =
    new BlobImplStream(aInputStream, aName, aContentType, aLastModifiedDate,
                       aLength);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

BlobImplStream::BlobImplStream(nsIInputStream* aInputStream,
                               const nsAString& aContentType,
                               uint64_t aLength)
  : BlobImplBase(aContentType, aLength)
  , mInputStream(aInputStream)
{
  mImmutable = true;
}

BlobImplStream::BlobImplStream(BlobImplStream* aOther,
                               const nsAString& aContentType,
                               uint64_t aStart, uint64_t aLength)
  : BlobImplBase(aContentType, aOther->mStart + aStart, aLength)
  , mInputStream(new SlicedInputStream(aOther->mInputStream, aStart, aLength))
{
  mImmutable = true;
}

BlobImplStream::BlobImplStream(nsIInputStream* aInputStream,
                               const nsAString& aName,
                               const nsAString& aContentType,
                               int64_t aLastModifiedDate,
                               uint64_t aLength)
  : BlobImplBase(aName, aContentType, aLength, aLastModifiedDate)
  , mInputStream(aInputStream)
{
  mImmutable = true;
}

BlobImplStream::~BlobImplStream()
{
  UnregisterWeakMemoryReporter(this);
}

void
BlobImplStream::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> clonedStream;
  nsCOMPtr<nsIInputStream> replacementStream;

  aRv = NS_CloneInputStream(mInputStream, getter_AddRefs(clonedStream),
                            getter_AddRefs(replacementStream));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (replacementStream) {
    mInputStream = replacementStream.forget();
  }

  clonedStream.forget(aStream);
}

already_AddRefed<BlobImpl>
BlobImplStream::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType, ErrorResult& aRv)
{
  if (!aLength) {
    RefPtr<BlobImpl> impl = new EmptyBlobImpl(aContentType);
    return impl.forget();
  }

  RefPtr<BlobImpl> impl =
    new BlobImplStream(this, aContentType, aStart, aLength);
  return impl.forget();
}

void
BlobImplStream::MaybeRegisterMemoryReporter()
{
  // We report only stringInputStream.
  nsCOMPtr<nsIStringInputStream> stringInputStream =
    do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return;
  }

  RegisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
BlobImplStream::CollectReports(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData, bool aAnonymize)
{
  nsCOMPtr<nsIStringInputStream> stringInputStream =
    do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return NS_OK;
  }

  MOZ_COLLECT_REPORT(
    "explicit/dom/memory-file-data/stream", KIND_HEAP, UNITS_BYTES,
    stringInputStream->SizeOfIncludingThis(MallocSizeOf),
    "Memory used to back a File/Blob based on an input stream.");

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
