/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_lockedfile_h__
#define mozilla_dom_file_lockedfile_h__

#include "FileCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"

namespace mozilla {
namespace dom {
class DOMFileMetadataParameters;
class DOMRequest;
} // namespace dom
} // namespace mozilla

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

BEGIN_FILE_NAMESPACE

class FileHandle;
class FileRequest;
class MetadataHelper;

class LockedFile : public nsDOMEventTargetHelper,
                   public nsIRunnable
{
  friend class FinishHelper;
  friend class FileService;
  friend class FileHelper;
  friend class MetadataHelper;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LockedFile, nsDOMEventTargetHelper)

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
  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  FileHandle* GetFileHandle() const
  {
    return Handle();
  }

  FileMode Mode() const
  {
    return mMode;
  }

  bool Active() const
  {
    return IsOpen();
  }

  Nullable<uint64_t> GetLocation() const
  {
    if (mLocation == UINT64_MAX) {
      return Nullable<uint64_t>();
    }
    return Nullable<uint64_t>(mLocation);
  }

  void SetLocation(const Nullable<uint64_t>& aLocation)
  {
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
    uint64_t length;
    nsCOMPtr<nsIInputStream> stream = GetInputStream(aValue, &length, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return Write(stream, length, aRv);
  }

  template<class T>
  already_AddRefed<FileRequest>
  Append(const T& aValue, ErrorResult& aRv)
  {
    uint64_t length;
    nsCOMPtr<nsIInputStream> stream = GetInputStream(aValue, &length, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return Append(stream, length, aRv);
  }

  already_AddRefed<FileRequest>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv);

  already_AddRefed<FileRequest>
  Flush(ErrorResult& aRv);

  void Abort(ErrorResult& aRv);

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

  bool CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv);
  bool CheckStateForWrite(ErrorResult& aRv);

  already_AddRefed<FileRequest>
  GenerateFileRequest();

  already_AddRefed<FileRequest>
  Write(nsIInputStream* aInputStream, uint64_t aInputLength, ErrorResult& aRv)
  {
    return WriteOrAppend(aInputStream, aInputLength, false, aRv);
  }

  already_AddRefed<FileRequest>
  Append(nsIInputStream* aInputStream, uint64_t aInputLength, ErrorResult& aRv)
  {
    return WriteOrAppend(aInputStream, aInputLength, true, aRv);
  }

  already_AddRefed<FileRequest>
  WriteOrAppend(nsIInputStream* aInputStream, uint64_t aInputLength,
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

END_FILE_NAMESPACE

#endif // mozilla_dom_file_lockedfile_h__
