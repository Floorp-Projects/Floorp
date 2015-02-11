/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultipartFileImpl.h"
#include "jsfriendapi.h"
#include "mozilla/dom/BlobSet.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsAutoPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsIMultiplexInputStream.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIXPConnect.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS_INHERITED0(MultipartFileImpl, FileImpl)

nsresult
MultipartFileImpl::GetInternalStream(nsIInputStream** aStream)
{
  nsresult rv;
  *aStream = nullptr;

  nsCOMPtr<nsIMultiplexInputStream> stream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  uint32_t i;
  for (i = 0; i < mBlobImpls.Length(); i++) {
    nsCOMPtr<nsIInputStream> scratchStream;
    FileImpl* blobImpl = mBlobImpls.ElementAt(i).get();

    rv = blobImpl->GetInternalStream(getter_AddRefs(scratchStream));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stream->AppendStream(scratchStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return CallQueryInterface(stream, aStream);
}

already_AddRefed<FileImpl>
MultipartFileImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType,
                               ErrorResult& aRv)
{
  // If we clamped to nothing we create an empty blob
  nsTArray<nsRefPtr<FileImpl>> blobImpls;

  uint64_t length = aLength;
  uint64_t skipStart = aStart;

  // Prune the list of blobs if we can
  uint32_t i;
  for (i = 0; length && skipStart && i < mBlobImpls.Length(); i++) {
    FileImpl* blobImpl = mBlobImpls[i].get();

    uint64_t l = blobImpl->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    if (skipStart < l) {
      uint64_t upperBound = std::min<uint64_t>(l - skipStart, length);

      nsRefPtr<FileImpl> firstBlobImpl =
        blobImpl->CreateSlice(skipStart, upperBound,
                              aContentType, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      // Avoid wrapping a single blob inside an MultipartFileImpl
      if (length == upperBound) {
        return firstBlobImpl.forget();
      }

      blobImpls.AppendElement(firstBlobImpl);
      length -= upperBound;
      i++;
      break;
    }
    skipStart -= l;
  }

  // Now append enough blobs until we're done
  for (; length && i < mBlobImpls.Length(); i++) {
    FileImpl* blobImpl = mBlobImpls[i].get();

    uint64_t l = blobImpl->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    if (length < l) {
      nsRefPtr<FileImpl> lastBlobImpl =
        blobImpl->CreateSlice(0, length, aContentType, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      blobImpls.AppendElement(lastBlobImpl);
    } else {
      blobImpls.AppendElement(blobImpl);
    }
    length -= std::min<uint64_t>(l, length);
  }

  // we can create our blob now
  nsRefPtr<FileImpl> impl =
    new MultipartFileImpl(blobImpls, aContentType);
  return impl.forget();
}

void
MultipartFileImpl::InitializeBlob()
{
  SetLengthAndModifiedDate();
}

void
MultipartFileImpl::InitializeBlob(
       JSContext* aCx,
       const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
       const nsAString& aContentType,
       bool aNativeEOL,
       ErrorResult& aRv)
{
  mContentType = aContentType;
  BlobSet blobSet;

  for (uint32_t i = 0, len = aData.Length(); i < len; ++i) {
    const OwningArrayBufferOrArrayBufferViewOrBlobOrString& data = aData[i];

    if (data.IsBlob()) {
      nsRefPtr<File> file = data.GetAsBlob().get();
      blobSet.AppendBlobImpl(file->Impl());
    }

    else if (data.IsString()) {
      aRv = blobSet.AppendString(data.GetAsString(), aNativeEOL, aCx);
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsArrayBuffer()) {
      const ArrayBuffer& buffer = data.GetAsArrayBuffer();
      buffer.ComputeLengthAndData();
      aRv = blobSet.AppendVoidPtr(buffer.Data(), buffer.Length());
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsArrayBufferView()) {
      const ArrayBufferView& buffer = data.GetAsArrayBufferView();
      buffer.ComputeLengthAndData();
      aRv = blobSet.AppendVoidPtr(buffer.Data(), buffer.Length());
      if (aRv.Failed()) {
        return;
      }
    }

    else {
      MOZ_CRASH("Impossible blob data type.");
    }
  }


  mBlobImpls = blobSet.GetBlobImpls();
  SetLengthAndModifiedDate();
}

void
MultipartFileImpl::SetLengthAndModifiedDate()
{
  MOZ_ASSERT(mLength == UINT64_MAX);
  MOZ_ASSERT(mLastModificationDate == UINT64_MAX);

  uint64_t totalLength = 0;
  uint64_t lastModified = 0;
  bool lastModifiedSet = false;

  for (uint32_t index = 0, count = mBlobImpls.Length(); index < count; index++) {
    nsRefPtr<FileImpl>& blob = mBlobImpls[index];

#ifdef DEBUG
    MOZ_ASSERT(!blob->IsSizeUnknown());
    MOZ_ASSERT(!blob->IsDateUnknown());
#endif

    ErrorResult error;
    uint64_t subBlobLength = blob->GetSize(error);
    MOZ_ALWAYS_TRUE(!error.Failed());

    MOZ_ASSERT(UINT64_MAX - subBlobLength >= totalLength);
    totalLength += subBlobLength;

    if (blob->IsFile()) {
      uint64_t partLastModified = blob->GetLastModified(error);
      MOZ_ALWAYS_TRUE(!error.Failed());

      if (lastModified < partLastModified) {
        lastModified = partLastModified;
        lastModifiedSet = true;
      }
    }
  }

  mLength = totalLength;

  if (mIsFile) {
    // We cannot use PR_Now() because bug 493756 and, for this reason:
    //   var x = new Date(); var f = new File(...);
    //   x.getTime() < f.dateModified.getTime()
    // could fail.
    mLastModificationDate =
      lastModifiedSet ? lastModified * PR_USEC_PER_MSEC : JS_Now();
  }
}

void
MultipartFileImpl::GetMozFullPathInternal(nsAString& aFilename,
                                          ErrorResult& aRv)
{
  if (!mIsFromNsIFile || mBlobImpls.Length() == 0) {
    FileImplBase::GetMozFullPathInternal(aFilename, aRv);
    return;
  }

  FileImpl* blobImpl = mBlobImpls.ElementAt(0).get();
  if (!blobImpl) {
    FileImplBase::GetMozFullPathInternal(aFilename, aRv);
    return;
  }

  blobImpl->GetMozFullPathInternal(aFilename, aRv);
}

nsresult
MultipartFileImpl::SetMutable(bool aMutable)
{
  nsresult rv;

  // This looks a little sketchy since FileImpl objects are supposed to be
  // threadsafe. However, we try to enforce that all FileImpl objects must be
  // set to immutable *before* being passed to another thread, so this should
  // be safe.
  if (!aMutable && !mImmutable && !mBlobImpls.IsEmpty()) {
    for (uint32_t index = 0, count = mBlobImpls.Length();
         index < count;
         index++) {
      rv = mBlobImpls[index]->SetMutable(aMutable);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  rv = FileImplBase::SetMutable(aMutable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!aMutable, mImmutable);

  return NS_OK;
}

void
MultipartFileImpl::InitializeChromeFile(File& aBlob,
                                        const ChromeFilePropertyBag& aBag,
                                        ErrorResult& aRv)
{
  NS_ASSERTION(!mImmutable, "Something went wrong ...");

  if (mImmutable) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  mName = aBag.mName;
  mContentType = aBag.mType;
  mIsFromNsIFile = true;

  // XXXkhuey this is terrible
  if (mContentType.IsEmpty()) {
    aBlob.GetType(mContentType);
  }


  BlobSet blobSet;
  blobSet.AppendBlobImpl(aBlob.Impl());
  mBlobImpls = blobSet.GetBlobImpls();

  SetLengthAndModifiedDate();
}

void
MultipartFileImpl::InitializeChromeFile(nsPIDOMWindow* aWindow,
                                        nsIFile* aFile,
                                        const ChromeFilePropertyBag& aBag,
                                        bool aIsFromNsIFile,
                                        ErrorResult& aRv)
{
  NS_ASSERTION(!mImmutable, "Something went wrong ...");
  if (mImmutable) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  mName = aBag.mName;
  mContentType = aBag.mType;
  mIsFromNsIFile = aIsFromNsIFile;

  bool exists;
  aRv = aFile->Exists(&exists);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!exists) {
    aRv.Throw(NS_ERROR_FILE_NOT_FOUND);
    return;
  }

  bool isDir;
  aRv = aFile->IsDirectory(&isDir);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (isDir) {
    aRv.Throw(NS_ERROR_FILE_IS_DIRECTORY);
    return;
  }

  if (mName.IsEmpty()) {
    aFile->GetLeafName(mName);
  }

  nsRefPtr<File> blob = File::CreateFromFile(aWindow, aFile, aBag.mTemporary);

  // Pre-cache size.
  uint64_t unused;
  aRv = blob->GetSize(&unused);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Pre-cache modified date.
  aRv = blob->GetMozLastModifiedDate(&unused);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // XXXkhuey this is terrible
  if (mContentType.IsEmpty()) {
    blob->GetType(mContentType);
  }

  BlobSet blobSet;
  blobSet.AppendBlobImpl(static_cast<File*>(blob.get())->Impl());
  mBlobImpls = blobSet.GetBlobImpls();

  SetLengthAndModifiedDate();
}

void
MultipartFileImpl::InitializeChromeFile(nsPIDOMWindow* aWindow,
                                        const nsAString& aData,
                                        const ChromeFilePropertyBag& aBag,
                                        ErrorResult& aRv)
{
  nsCOMPtr<nsIFile> file;
  aRv = NS_NewLocalFile(aData, false, getter_AddRefs(file));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  InitializeChromeFile(aWindow, file, aBag, false, aRv);
}

bool
MultipartFileImpl::MayBeClonedToOtherThreads() const
{
  for (uint32_t i = 0; i < mBlobImpls.Length(); ++i) {
    if (!mBlobImpls[i]->MayBeClonedToOtherThreads()) {
      return false;
    }
  }

  return true;
}
