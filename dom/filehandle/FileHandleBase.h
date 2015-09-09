/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHandle_h
#define mozilla_dom_FileHandle_h

#include "FileHandleCommon.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/TypedArray.h"

template <class> struct already_AddRefed;
class nsAString;
struct PRThread;

namespace mozilla {

class ErrorResult;

namespace dom {

class BackgroundFileHandleChild;
class Blob;
class FileRequestBase;
class FileRequestData;
class FileRequestParams;
class MutableFileBase;
class StringOrArrayBufferOrArrayBufferViewOrBlob;

/**
 * This class provides a base for FileHandle implementations.
 */
class FileHandleBase
  : public RefCountedThreadObject
{
public:
  enum ReadyState
  {
    INITIAL = 0,
    LOADING,
    FINISHING,
    DONE
  };

private:
  BackgroundFileHandleChild* mBackgroundActor;

  uint64_t mLocation;

  uint32_t mPendingRequestCount;

  ReadyState mReadyState;
  FileMode mMode;

  bool mAborted;
  bool mCreating;

  DEBUGONLY(bool mSentFinishOrAbort;)
  DEBUGONLY(bool mFiredCompleteOrAbort;)

public:
  static FileHandleBase*
  GetCurrent();

  void
  SetBackgroundActor(BackgroundFileHandleChild* aActor);

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  void
  StartRequest(FileRequestBase* aFileRequest, const FileRequestParams& aParams);

  void
  OnNewRequest();

  void
  OnRequestFinished(bool aActorDestroyedNormally);

  bool
  IsOpen() const;

  bool
  IsFinishingOrDone() const
  {
    AssertIsOnOwningThread();

    return mReadyState == FINISHING || mReadyState == DONE;
  }

  bool
  IsDone() const
  {
    AssertIsOnOwningThread();

    return mReadyState == DONE;
  }

  bool
  IsAborted() const
  {
    AssertIsOnOwningThread();
    return mAborted;
  }

  void
  SetCreating()
  {
    mCreating = true;
  }

  void
  Abort();

  // Shared WebIDL (IndexedDB FileHandle and FileSystem FileHandle)
  FileMode
  Mode() const
  {
    AssertIsOnOwningThread();
    return mMode;
  }

  bool
  Active() const
  {
    AssertIsOnOwningThread();
    return IsOpen();
  }

  Nullable<uint64_t>
  GetLocation() const
  {
    AssertIsOnOwningThread();

    if (mLocation == UINT64_MAX) {
      return Nullable<uint64_t>();
    }

    return Nullable<uint64_t>(mLocation);
  }

  void
  SetLocation(const Nullable<uint64_t>& aLocation)
  {
    AssertIsOnOwningThread();

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

  //  Must be overridden in subclasses.
  virtual MutableFileBase*
  MutableFile() const = 0;

  //  May be overridden in subclasses.
  virtual void
  HandleCompleteOrAbort(bool aAborted);

protected:
  FileHandleBase(DEBUGONLY(PRThread* aOwningThread,)
                 FileMode aMode);

  ~FileHandleBase();

  void
  OnReturnToEventLoop();

  bool
  CheckState(ErrorResult& aRv);

  bool
  CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv);

  bool
  CheckStateForWrite(ErrorResult& aRv);

  bool
  CheckStateForWriteOrAppend(bool aAppend, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  WriteOrAppend(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
                bool aAppend,
                ErrorResult& aRv);

  //  Must be overridden in subclasses.
  virtual bool
  CheckWindow() = 0;

  //  Must be overridden in subclasses.
  virtual already_AddRefed<FileRequestBase>
  GenerateFileRequest() = 0;

private:
  already_AddRefed<FileRequestBase>
  WriteOrAppend(const nsAString& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  WriteOrAppend(const ArrayBuffer& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  WriteOrAppend(const ArrayBufferView& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  WriteOrAppend(Blob& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<FileRequestBase>
  WriteInternal(const FileRequestData& aData, uint64_t aDataLength,
                bool aAppend, ErrorResult& aRv);

  void
  SendFinish();

  void
  SendAbort();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHandle_h
