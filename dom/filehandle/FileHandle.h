/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHandle_h
#define mozilla_dom_FileHandle_h

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsTArray.h"

class nsAString;

namespace mozilla {
namespace dom {

class DOMFile;
class FileHelper;
class FileRequestBase;
class FileService;
class FinishHelper;
class MetadataHelper;
class MutableFileBase;

/**
 * This class provides a base for FileHandle implementations.
 */
class FileHandleBase
{
public:
  enum RequestMode
  {
    NORMAL = 0, // Sequential
    PARALLEL
  };

  enum ReadyState
  {
    INITIAL = 0,
    LOADING,
    FINISHING,
    DONE
  };

private:
  friend class FileHelper;
  friend class FileService;
  friend class FinishHelper;
  friend class MetadataHelper;

  ReadyState mReadyState;
  FileMode mMode;
  RequestMode mRequestMode;
  uint64_t mLocation;
  uint32_t mPendingRequests;

  nsTArray<nsCOMPtr<nsISupports>> mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
  bool mCreating;

public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;

  nsresult
  CreateParallelStream(nsISupports** aStream);

  nsresult
  GetOrCreateStream(nsISupports** aStream);

  bool
  IsOpen() const;

  bool
  IsAborted() const
  {
    return mAborted;
  }

  void
  SetCreating()
  {
    mCreating = true;
  }

  virtual MutableFileBase*
  MutableFile() const = 0;

  nsresult
  OpenInputStream(bool aWholeFile, uint64_t aStart, uint64_t aLength,
                  nsIInputStream** aResult);

  // Shared WebIDL (IndexedDB FileHandle and FileSystem FileHandle)
  FileMode
  Mode() const
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return mMode;
  }

  bool
  Active() const
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return IsOpen();
  }

  Nullable<uint64_t>
  GetLocation() const
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    if (mLocation == UINT64_MAX) {
      return Nullable<uint64_t>();
    }

    return Nullable<uint64_t>(mLocation);
  }

  void
  SetLocation(const Nullable<uint64_t>& aLocation)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    // Null means the end-of-file.
    if (aLocation.IsNull()) {
      mLocation = UINT64_MAX;
    } else {
      mLocation = aLocation.Value();
    }
  }

  already_AddRefed<FileRequestBase>
  Read(uint64_t aSize, bool aHasEncoding, const nsAString& aEncoding,
       ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  Flush(ErrorResult& aRv);

  void
  Abort(ErrorResult& aRv);

protected:
  FileHandleBase(FileMode aMode,
                 RequestMode aRequestMode);
  ~FileHandleBase();

  void
  OnNewRequest();

  void
  OnRequestFinished();

  void
  OnReturnToEventLoop();

  virtual nsresult
  OnCompleteOrAbort(bool aAborted) = 0;

  bool
  CheckState(ErrorResult& aRv);

  bool
  CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv);

  bool
  CheckStateForWrite(ErrorResult& aRv);

  virtual bool
  CheckWindow() = 0;

  virtual already_AddRefed<FileRequestBase>
  GenerateFileRequest() = 0;

  template<class T>
  already_AddRefed<FileRequestBase>
  WriteOrAppend(const T& aValue, bool aAppend, ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    // State checking for write
    if (!CheckStateForWrite(aRv)) {
      return nullptr;
    }

    // Additional state checking for write
    if (!aAppend && mLocation == UINT64_MAX) {
      aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
      return nullptr;
    }

    uint64_t length;
    nsCOMPtr<nsIInputStream> stream = GetInputStream(aValue, &length, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    if (!length) {
      return nullptr;
    }

    // Do nothing if the window is closed
    if (!CheckWindow()) {
      return nullptr;
    }

    return WriteInternal(stream, length, aAppend, aRv);
  }

  already_AddRefed<FileRequestBase>
  WriteInternal(nsIInputStream* aInputStream, uint64_t aInputLength,
                bool aAppend, ErrorResult& aRv);

  nsresult
  Finish();

  static already_AddRefed<nsIInputStream>
  GetInputStream(const ArrayBuffer& aValue, uint64_t* aInputLength,
                 ErrorResult& aRv);

  static already_AddRefed<nsIInputStream>
  GetInputStream(const DOMFile& aValue, uint64_t* aInputLength,
                 ErrorResult& aRv);

  static already_AddRefed<nsIInputStream>
  GetInputStream(const nsAString& aValue, uint64_t* aInputLength,
                 ErrorResult& aRv);
};

class FinishHelper MOZ_FINAL : public nsIRunnable
{
  friend class FileHandleBase;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

private:
  explicit FinishHelper(FileHandleBase* aFileHandle);
  ~FinishHelper()
  { }

  nsRefPtr<FileHandleBase> mFileHandle;
  nsTArray<nsCOMPtr<nsISupports>> mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHandle_h
