/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbfilehandle_h__
#define mozilla_dom_idbfilehandle_h__

#include "IDBFileRequest.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileModeBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsWeakReference.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Blob;
class FileRequestData;
class FileRequestParams;
struct IDBFileMetadataParameters;
class IDBFileRequest;
class IDBMutableFile;
class StringOrArrayBufferOrArrayBufferViewOrBlob;

namespace indexedDB {
class BackgroundFileHandleChild;
}

class IDBFileHandle final
  : public DOMEventTargetHelper
  , public nsIRunnable
  , public nsSupportsWeakReference
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
  RefPtr<IDBMutableFile> mMutableFile;

  indexedDB::BackgroundFileHandleChild* mBackgroundActor;

  uint64_t mLocation;

  uint32_t mPendingRequestCount;

  ReadyState mReadyState;
  FileMode mMode;

  bool mAborted;
  bool mCreating;

#ifdef DEBUG
  bool mSentFinishOrAbort;
  bool mFiredCompleteOrAbort;
#endif

public:
  static already_AddRefed<IDBFileHandle>
  Create(IDBMutableFile* aMutableFile,
         FileMode aMode);

  static IDBFileHandle*
  GetCurrent();

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  void
  SetBackgroundActor(indexedDB::BackgroundFileHandleChild* aActor);

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  void
  StartRequest(IDBFileRequest* aFileRequest, const FileRequestParams& aParams);

  void
  OnNewRequest();

  void
  OnRequestFinished(bool aActorDestroyedNormally);

  void
  FireCompleteOrAbortEvents(bool aAborted);

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
  Abort();

  // WebIDL
  nsPIDOMWindowInner*
  GetParentObject() const
  {
    AssertIsOnOwningThread();
    return GetOwner();
  }

  IDBMutableFile*
  GetMutableFile() const
  {
    AssertIsOnOwningThread();
    return mMutableFile;
  }

  IDBMutableFile*
  GetFileHandle() const
  {
    AssertIsOnOwningThread();
    return GetMutableFile();
  }

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

  already_AddRefed<IDBFileRequest>
  GetMetadata(const IDBFileMetadataParameters& aParameters, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  ReadAsArrayBuffer(uint64_t aSize, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return Read(aSize, false, NullString(), aRv);
  }

  already_AddRefed<IDBFileRequest>
  ReadAsText(uint64_t aSize, const nsAString& aEncoding, ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return Read(aSize, true, aEncoding, aRv);
  }

  already_AddRefed<IDBFileRequest>
  Write(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
        ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return WriteOrAppend(aValue, false, aRv);
  }

  already_AddRefed<IDBFileRequest>
  Append(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
         ErrorResult& aRv)
  {
    AssertIsOnOwningThread();
    return WriteOrAppend(aValue, true, aRv);
  }

  already_AddRefed<IDBFileRequest>
  Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  Flush(ErrorResult& aRv);

  void
  Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileHandle, DOMEventTargetHelper)

  // nsIDOMEventTarget
  virtual nsresult
  GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBFileHandle(IDBMutableFile* aMutableFile,
                FileMode aMode);
  ~IDBFileHandle();

  bool
  CheckState(ErrorResult& aRv);

  bool
  CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv);

  bool
  CheckStateForWrite(ErrorResult& aRv);

  bool
  CheckStateForWriteOrAppend(bool aAppend, ErrorResult& aRv);

  bool
  CheckWindow();

  already_AddRefed<IDBFileRequest>
  Read(uint64_t aSize, bool aHasEncoding, const nsAString& aEncoding,
       ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteOrAppend(const StringOrArrayBufferOrArrayBufferViewOrBlob& aValue,
                bool aAppend,
                ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteOrAppend(const nsAString& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteOrAppend(const ArrayBuffer& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteOrAppend(const ArrayBufferView& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteOrAppend(Blob& aValue, bool aAppend, ErrorResult& aRv);

  already_AddRefed<IDBFileRequest>
  WriteInternal(const FileRequestData& aData, uint64_t aDataLength,
                bool aAppend, ErrorResult& aRv);

  void
  SendFinish();

  void
  SendAbort();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbfilehandle_h__
