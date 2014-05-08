/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LockedFile_h
#define mozilla_dom_LockedFile_h

#include "js/TypeDecls.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsTArray.h"

class nsAString;
class nsIDOMBlob;
class nsPIDOMWindow;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class DOMFileMetadataParameters;
class FileHandle;
class FileHelper;
class FileRequest;
class FileService;
class FinishHelper;
class MetadataHelper;

class LockedFile : public DOMEventTargetHelper,
                   public nsIRunnable
{
  friend class FileHelper;
  friend class FileService;
  friend class FinishHelper;
  friend class MetadataHelper;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LockedFile, DOMEventTargetHelper)

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

  static already_AddRefed<LockedFile>
  Create(FileHandle* aFileHandle,
         FileMode aMode,
         RequestMode aRequestMode = NORMAL);

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

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

  FileHandle*
  Handle() const
  {
    return mFileHandle;
  }

  nsresult
  OpenInputStream(bool aWholeFile, uint64_t aStart, uint64_t aLength,
                  nsIInputStream** aResult);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  FileHandle*
  GetFileHandle() const
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return Handle();
  }

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

  already_AddRefed<FileRequest>
  GetMetadata(const DOMFileMetadataParameters& aParameters, ErrorResult& aRv);

  already_AddRefed<FileRequest>
  ReadAsArrayBuffer(uint64_t aSize, ErrorResult& aRv);

  already_AddRefed<FileRequest>
  ReadAsText(uint64_t aSize, const nsAString& aEncoding, ErrorResult& aRv);

  template<class T>
  already_AddRefed<FileRequest>
  Write(const T& aValue, ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return WriteOrAppend(aValue, false, aRv);
  }

  template<class T>
  already_AddRefed<FileRequest>
  Append(const T& aValue, ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    return WriteOrAppend(aValue, true, aRv);
  }

  already_AddRefed<FileRequest>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv);

  already_AddRefed<FileRequest>
  Flush(ErrorResult& aRv);

  void
  Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

private:
  LockedFile();
  ~LockedFile();

  void
  OnNewRequest();

  void
  OnRequestFinished();

  bool
  CheckState(ErrorResult& aRv);

  bool
  CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv);

  bool
  CheckStateForWrite(ErrorResult& aRv);

  already_AddRefed<FileRequest>
  GenerateFileRequest();

  template<class T>
  already_AddRefed<FileRequest>
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
    if (!GetOwner()) {
      return nullptr;
    }

    return WriteInternal(stream, length, aAppend, aRv);
  }

  already_AddRefed<FileRequest>
  WriteInternal(nsIInputStream* aInputStream, uint64_t aInputLength,
                bool aAppend, ErrorResult& aRv);

  nsresult
  Finish();

  static already_AddRefed<nsIInputStream>
  GetInputStream(const ArrayBuffer& aValue, uint64_t* aInputLength,
                 ErrorResult& aRv);

  static already_AddRefed<nsIInputStream>
  GetInputStream(nsIDOMBlob* aValue, uint64_t* aInputLength, ErrorResult& aRv);

  static already_AddRefed<nsIInputStream>
  GetInputStream(const nsAString& aValue, uint64_t* aInputLength,
                 ErrorResult& aRv);

  nsRefPtr<FileHandle> mFileHandle;
  ReadyState mReadyState;
  FileMode mMode;
  RequestMode mRequestMode;
  uint64_t mLocation;
  uint32_t mPendingRequests;

  nsTArray<nsCOMPtr<nsISupports> > mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
  bool mCreating;
};

class FinishHelper MOZ_FINAL : public nsIRunnable
{
  friend class LockedFile;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

private:
  FinishHelper(LockedFile* aLockedFile);
  ~FinishHelper()
  { }

  nsRefPtr<LockedFile> mLockedFile;
  nsTArray<nsCOMPtr<nsISupports> > mParallelStreams;
  nsCOMPtr<nsISupports> mStream;

  bool mAborted;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_LockedFile_h
