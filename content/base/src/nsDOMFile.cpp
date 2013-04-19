/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFile.h"

#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsICharsetDetector.h"
#include "nsICharsetConverterManager.h"
#include "nsIClassInfo.h"
#include "nsIConverterInputStream.h"
#include "nsIDocument.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMemoryReporter.h"
#include "nsIMIMEService.h"
#include "nsIPlatformCharset.h"
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

#include "mozilla/dom/FileListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// XXXkhuey the input stream that we pass out of a DOMFile
// can outlive the actual DOMFile object.  Thus, we must
// ensure that the buffer underlying the stream we get
// from NS_NewByteInputStream is held alive as long as the
// stream is.  We do that by passing back this class instead.
class DataOwnerAdapter MOZ_FINAL : public nsIInputStream,
                                   public nsISeekableStream,
                                   public nsIIPCSerializableInputStream
{
  typedef nsDOMMemoryFile::DataOwner DataOwner;
public:
  static nsresult Create(DataOwner* aDataOwner,
                         uint32_t aStart,
                         uint32_t aLength,
                         nsIInputStream** _retval);

  NS_DECL_ISUPPORTS

  // These are mandatory.
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_NSISEEKABLESTREAM(mSeekableStream->)

  // This is optional. We use a conditional QI to keep it from being called
  // if the underlying stream doesn't support it.
  NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(mSerializableInputStream->)

private:
  DataOwnerAdapter(DataOwner* aDataOwner,
                   nsIInputStream* aStream)
    : mDataOwner(aDataOwner), mStream(aStream),
      mSeekableStream(do_QueryInterface(aStream)),
      mSerializableInputStream(do_QueryInterface(aStream))
  {
    NS_ASSERTION(mSeekableStream, "Somebody gave us the wrong stream!");
  }

  nsRefPtr<DataOwner> mDataOwner;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializableInputStream;
};

NS_IMPL_THREADSAFE_ADDREF(DataOwnerAdapter)
NS_IMPL_THREADSAFE_RELEASE(DataOwnerAdapter)

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
  NS_ASSERTION(aDataOwner, "Uh ...");

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
// nsDOMFileBase implementation

NS_IMETHODIMP
nsDOMFileBase::GetName(nsAString &aFileName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aFileName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetLastModifiedDate(JSContext* cx, JS::Value *aLastModifiedDate)
{
  JSObject* date = JS_NewDateObjectMsec(cx, JS_Now() / PR_USEC_PER_MSEC);
  aLastModifiedDate->setObject(*date);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetMozFullPath(nsAString &aFileName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");

  // It is unsafe to call IsCallerChrome on a non-main thread. If
  // you hit the following assertion you need to figure out some other way to
  // determine privileges and call GetMozFullPathInternal.
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (nsContentUtils::IsCallerChrome()) {
    return GetMozFullPathInternal(aFileName);
  }
  aFileName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetMozFullPathInternal(nsAString &aFileName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aFileName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetSize(uint64_t *aSize)
{
  *aSize = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetType(nsAString &aType)
{
  aType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetMozLastModifiedDate(uint64_t* aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = PR_Now();
  }
  *aLastModifiedDate = mLastModificationDate;
  return NS_OK;
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

NS_IMETHODIMP
nsDOMFileBase::Slice(int64_t aStart, int64_t aEnd,
                     const nsAString& aContentType, uint8_t optional_argc,
                     nsIDOMBlob **aBlob)
{
  *aBlob = nullptr;

  // Truncate aStart and aEnd so that we stay within this file.
  uint64_t thisLength;
  nsresult rv = GetSize(&thisLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (optional_argc < 2) {
    aEnd = (int64_t)thisLength;
  }

  ParseSize((int64_t)thisLength, aStart, aEnd);
  
  // Create the new file
  *aBlob = CreateSlice((uint64_t)aStart, (uint64_t)(aEnd - aStart),
                       aContentType).get();

  return *aBlob ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMFileBase::MozSlice(int64_t aStart, int64_t aEnd,
                        const nsAString& aContentType, 
                        JSContext* aCx,
                        uint8_t optional_argc,
                        nsIDOMBlob **aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsIScriptGlobalObject* sgo = nsJSUtils::GetDynamicScriptGlobal(aCx);
  if (sgo) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(sgo);
    if (window) {
      nsCOMPtr<nsIDocument> document =
        do_QueryInterface(window->GetExtantDocument());
      if (document) {
        document->WarnOnceAbout(nsIDocument::eMozSlice);
      }
    }
  }

  return Slice(aStart, aEnd, aContentType, optional_argc, aBlob);
}

NS_IMETHODIMP
nsDOMFileBase::GetInternalStream(nsIInputStream **aStream)
{
  // Must be overridden
  NS_NOTREACHED("Must override GetInternalStream");
  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMFileBase::GetInternalUrl(nsIPrincipal* aPrincipal, nsAString& aURL)
{
  NS_ENSURE_STATE(aPrincipal);

  nsCString url;
  nsresult rv = nsBlobProtocolHandler::AddDataEntry(
    NS_LITERAL_CSTRING(BLOBURI_SCHEME),
    static_cast<nsIDOMBlob*>(this), aPrincipal, url);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CopyASCIItoUTF16(url, aURL);
  return NS_OK;
}

NS_IMETHODIMP_(int64_t)
nsDOMFileBase::GetFileId()
{
  int64_t id = -1;

  if (IsStoredFile() && IsWholeFile() && !IsSnapshot()) {
    if (!indexedDB::IndexedDatabaseManager::IsClosed()) {
      indexedDB::IndexedDatabaseManager::FileMutex().Lock();
    }

    NS_ASSERTION(!mFileInfos.IsEmpty(),
                 "A stored file must have at least one file info!");

    nsRefPtr<indexedDB::FileInfo>& fileInfo = mFileInfos.ElementAt(0);
    if (fileInfo) {
      id =  fileInfo->Id();
    }

    if (!indexedDB::IndexedDatabaseManager::IsClosed()) {
      indexedDB::IndexedDatabaseManager::FileMutex().Unlock();
    }
  }

  return id;
}

NS_IMETHODIMP_(void)
nsDOMFileBase::AddFileInfo(indexedDB::FileInfo* aFileInfo)
{
  if (indexedDB::IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return;
  }

  nsRefPtr<indexedDB::FileInfo> fileInfo = aFileInfo;

  MutexAutoLock lock(indexedDB::IndexedDatabaseManager::FileMutex());

  NS_ASSERTION(!mFileInfos.Contains(aFileInfo),
               "Adding the same file info agan?!");

  nsRefPtr<indexedDB::FileInfo>* element = mFileInfos.AppendElement();
  element->swap(fileInfo);
}

NS_IMETHODIMP_(indexedDB::FileInfo*)
nsDOMFileBase::GetFileInfo(indexedDB::FileManager* aFileManager)
{
  if (indexedDB::IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return nullptr;
  }

  // A slice created from a stored file must keep the file info alive.
  // However, we don't support sharing of slices yet, so the slice must be
  // copied again. That's why we have to ignore the first file info.
  // Snapshots are handled in a similar way (they have to be copied).
  uint32_t startIndex;
  if (IsStoredFile() && (!IsWholeFile() || IsSnapshot())) {
    startIndex = 1;
  }
  else {
    startIndex = 0;
  }

  MutexAutoLock lock(indexedDB::IndexedDatabaseManager::FileMutex());

  for (uint32_t i = startIndex; i < mFileInfos.Length(); i++) {
    nsRefPtr<indexedDB::FileInfo>& fileInfo = mFileInfos.ElementAt(i);
    if (fileInfo->Manager() == aFileManager) {
      return fileInfo;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMFileBase::GetSendInfo(nsIInputStream** aBody,
                           uint64_t* aContentLength,
                           nsACString& aContentType,
                           nsACString& aCharset)
{
  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = this->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = this->GetSize(aContentLength);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString contentType;
  rv = this->GetType(contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(contentType, aContentType);

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::GetMutable(bool* aMutable)
{
  *aMutable = !mImmutable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileBase::SetMutable(bool aMutable)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(!mImmutable || !aMutable);

  if (!mImmutable && !aMutable) {
    // Force the content type and size to be cached
    nsString dummyString;
    rv = this->GetType(dummyString);
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t dummyInt;
    rv = this->GetSize(&dummyInt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mImmutable = !aMutable;
  return rv;
}

////////////////////////////////////////////////////////////////////////////
// nsDOMFile implementation

DOMCI_DATA(File, nsDOMFile)
DOMCI_DATA(Blob, nsDOMFile)

NS_INTERFACE_MAP_BEGIN(nsDOMFile)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, mIsFile)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(File, mIsFile)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(Blob, !mIsFile)
NS_INTERFACE_MAP_END

// Threadsafe when GetMutable() == false
NS_IMPL_THREADSAFE_ADDREF(nsDOMFile)
NS_IMPL_THREADSAFE_RELEASE(nsDOMFile)

////////////////////////////////////////////////////////////////////////////
// nsDOMFileCC implementation

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsDOMFileCC)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMFileCC)
  // We don't have anything to traverse, but some of our subclasses do.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMFileCC)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, mIsFile)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(File, mIsFile)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(Blob, !mIsFile)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMFileCC)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMFileCC)

////////////////////////////////////////////////////////////////////////////
// nsDOMFileFile implementation

already_AddRefed<nsIDOMBlob>
nsDOMFileFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                           const nsAString& aContentType)
{
  nsCOMPtr<nsIDOMBlob> t = new nsDOMFileFile(this, aStart, aLength, aContentType);
  return t.forget();
}

NS_IMETHODIMP
nsDOMFileFile::GetMozFullPathInternal(nsAString &aFilename)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  return mFile->GetPath(aFilename);
}

NS_IMETHODIMP
nsDOMFileFile::GetLastModifiedDate(JSContext* cx, JS::Value* aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");

  PRTime msecs;
  if (IsDateUnknown()) {
    nsresult rv = mFile->GetLastModifiedTime(&msecs);
    NS_ENSURE_SUCCESS(rv, rv);
    mLastModificationDate = msecs;
  } else {
    msecs = mLastModificationDate;
  }

  JSObject* date = JS_NewDateObjectMsec(cx, msecs);
  if (date) {
    aLastModifiedDate->setObject(*date);
  }
  else {
    date = JS_NewDateObjectMsec(cx, JS_Now() / PR_USEC_PER_MSEC);
    aLastModifiedDate->setObject(*date);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileFile::GetSize(uint64_t *aFileSize)
{
  if (IsSizeUnknown()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy size when using the whole file");
    int64_t fileSize;
    nsresult rv = mFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);
  
    if (fileSize < 0) {
      return NS_ERROR_FAILURE;
    }
  
    mLength = fileSize;
  }

  *aFileSize = mLength;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileFile::GetType(nsAString &aType)
{
  if (mContentType.IsVoid()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy ContentType when using the whole file");
    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString mimeType;
    rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      mimeType.Truncate();
    }

    AppendUTF8toUTF16(mimeType, mContentType);
    mContentType.SetIsVoid(false);
  }

  aType = mContentType;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileFile::GetMozLastModifiedDate(uint64_t* aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    PRTime msecs;
    nsresult rv = mFile->GetLastModifiedTime(&msecs);
    NS_ENSURE_SUCCESS(rv, rv);
    mLastModificationDate = msecs;
  }
  *aLastModifiedDate = mLastModificationDate;
  return NS_OK;
}

const uint32_t sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN;

NS_IMETHODIMP
nsDOMFileFile::GetInternalStream(nsIInputStream **aStream)
{
  return mWholeFile ?
    NS_NewLocalFileInputStream(aStream, mFile, -1, -1, sFileStreamFlags) :
    NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                      -1, -1, sFileStreamFlags);
}

////////////////////////////////////////////////////////////////////////////
// nsDOMMemoryFile implementation

already_AddRefed<nsIDOMBlob>
nsDOMMemoryFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                             const nsAString& aContentType)
{
  nsCOMPtr<nsIDOMBlob> t =
    new nsDOMMemoryFile(this, aStart, aLength, aContentType);
  return t.forget();
}

NS_IMETHODIMP
nsDOMMemoryFile::GetInternalStream(nsIInputStream **aStream)
{
  if (mLength > INT32_MAX)
    return NS_ERROR_FAILURE;

  return DataOwnerAdapter::Create(mDataOwner, mStart, mLength, aStream);
}

/* static */ StaticAutoPtr<LinkedList<nsDOMMemoryFile::DataOwner> >
nsDOMMemoryFile::DataOwner::sDataOwners;

/* static */ bool
nsDOMMemoryFile::DataOwner::sMemoryReporterRegistered;

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(DOMMemoryFileDataOwnerMallocSizeOf)

class nsDOMMemoryFileDataOwnerMemoryReporter MOZ_FINAL
  : public nsIMemoryMultiReporter
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetName(nsACString& aName)
  {
    aName.AssignASCII("dom-memory-file-data-owner");
    return NS_OK;
  }

  NS_IMETHOD CollectReports(nsIMemoryMultiReporterCallback *aCallback,
                            nsISupports *aClosure)
  {
    typedef nsDOMMemoryFile::DataOwner DataOwner;

    if (!DataOwner::sDataOwners) {
      return NS_OK;
    }

    const size_t LARGE_OBJECT_MIN_SIZE = 8 * 1024;
    size_t smallObjectsTotal = 0;

    for (DataOwner *owner = DataOwner::sDataOwners->getFirst();
         owner; owner = owner->getNext()) {

      size_t size = DOMMemoryFileDataOwnerMallocSizeOf(owner->mData);

      if (size < LARGE_OBJECT_MIN_SIZE) {
        smallObjectsTotal += size;
      }
      else {
        SHA1Sum sha1;
        sha1.update(owner->mData, owner->mLength);
        uint8_t digest[SHA1Sum::HashSize]; // SHA1 digests are 20 bytes long.
        sha1.finish(digest);

        nsAutoCString digestString;
        for (size_t i = 0; i < sizeof(digest); i++) {
          digestString.AppendPrintf("%02x", digest[i]);
        }

        nsresult rv = aCallback->Callback(
          /* process */ NS_LITERAL_CSTRING(""),
          nsPrintfCString(
            "explicit/dom/memory-file-data/large/file(length=%llu, sha1=%s)",
            owner->mLength, digestString.get()),
          nsIMemoryReporter::KIND_HEAP,
          nsIMemoryReporter::UNITS_BYTES,
          size,
          nsPrintfCString(
            "Memory used to back a memory file of length %llu bytes.  The file "
            "has a sha1 of %s.\n\n"
            "Note that the allocator may round up a memory file's length -- "
            "that is, an N-byte memory file may take up more than N bytes of "
            "memory.",
            owner->mLength, digestString.get()),
          aClosure);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    if (smallObjectsTotal > 0) {
      nsresult rv = aCallback->Callback(
        /* process */ NS_LITERAL_CSTRING(""),
        NS_LITERAL_CSTRING("explicit/dom/memory-file-data/small"),
        nsIMemoryReporter::KIND_HEAP,
        nsIMemoryReporter::UNITS_BYTES,
        smallObjectsTotal,
        nsPrintfCString(
          "Memory used to back small memory files (less than %d bytes each).\n\n"
          "Note that the allocator may round up a memory file's length -- "
          "that is, an N-byte memory file may take up more than N bytes of "
          "memory."),
        aClosure);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(nsDOMMemoryFileDataOwnerMemoryReporter,
                   nsIMemoryMultiReporter)

/* static */ void
nsDOMMemoryFile::DataOwner::EnsureMemoryReporterRegistered()
{
  if (sMemoryReporterRegistered) {
    return;
  }

  nsRefPtr<nsDOMMemoryFileDataOwnerMemoryReporter> reporter = new
    nsDOMMemoryFileDataOwnerMemoryReporter();
  NS_RegisterMemoryMultiReporter(reporter);

  sMemoryReporterRegistered = true;
}

////////////////////////////////////////////////////////////////////////////
// nsDOMFileList implementation

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsDOMFileList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMFileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMFileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMFileList)

JSObject*
nsDOMFileList::WrapObject(JSContext *cx, JSObject *scope)
{
  return FileListBinding::Wrap(cx, scope, this);
}

NS_IMETHODIMP
nsDOMFileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileList::Item(uint32_t aIndex, nsIDOMFile **aFile)
{
  NS_IF_ADDREF(*aFile = Item(aIndex));

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// nsDOMFileInternalUrlHolder implementation

nsDOMFileInternalUrlHolder::nsDOMFileInternalUrlHolder(nsIDOMBlob* aFile,
                                                       nsIPrincipal* aPrincipal
                                                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  aFile->GetInternalUrl(aPrincipal, mUrl);
}
 
nsDOMFileInternalUrlHolder::~nsDOMFileInternalUrlHolder() {
  if (!mUrl.IsEmpty()) {
    nsAutoCString narrowUrl;
    CopyUTF16toUTF8(mUrl, narrowUrl);
    nsBlobProtocolHandler::RemoveDataEntry(narrowUrl);
  }
}
