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
#include "mozilla/dom/TypedArray.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRunnable.h"
#include "nsWeakReference.h"

namespace mozilla::dom {

class Blob;
class FileRequestData;
class FileRequestParams;
struct IDBFileMetadataParameters;
class IDBFileRequest;
class IDBMutableFile;
class StringOrArrayBufferOrArrayBufferViewOrBlob;

class IDBFileHandle final : public DOMEventTargetHelper,
                            public nsIRunnable,
                            public nsSupportsWeakReference {
 public:
  enum ReadyState { INITIAL = 0, LOADING, FINISHING, DONE };

 private:
  RefPtr<IDBMutableFile> mMutableFile;

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
  [[nodiscard]] static RefPtr<IDBFileHandle> Create(
      IDBMutableFile* aMutableFile, FileMode aMode);

  static IDBFileHandle* GetCurrent();

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void OnRequestFinished(bool aActorDestroyedNormally);

  void FireCompleteOrAbortEvents(bool aAborted);

  bool IsOpen() const;

  bool IsFinishingOrDone() const {
    AssertIsOnOwningThread();

    return mReadyState == FINISHING || mReadyState == DONE;
  }

  bool IsDone() const {
    AssertIsOnOwningThread();

    return mReadyState == DONE;
  }

  bool IsAborted() const {
    AssertIsOnOwningThread();
    return mAborted;
  }

  void Abort();

  // WebIDL
  IDBMutableFile* GetMutableFile() const {
    AssertIsOnOwningThread();
    return mMutableFile;
  }

  IDBMutableFile* GetFileHandle() const {
    AssertIsOnOwningThread();
    return GetMutableFile();
  }

  FileMode Mode() const {
    AssertIsOnOwningThread();
    return mMode;
  }

  bool Active() const {
    AssertIsOnOwningThread();
    return IsOpen();
  }

  Nullable<uint64_t> GetLocation() const {
    AssertIsOnOwningThread();

    if (mLocation == UINT64_MAX) {
      return Nullable<uint64_t>();
    }

    return Nullable<uint64_t>(mLocation);
  }

  void SetLocation(const Nullable<uint64_t>& aLocation) {
    AssertIsOnOwningThread();

    // Null means the end-of-file.
    if (aLocation.IsNull()) {
      mLocation = UINT64_MAX;
    } else {
      mLocation = aLocation.Value();
    }
  }

  void Abort(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(complete)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFileHandle, DOMEventTargetHelper)

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // WrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return nullptr;
  }

 private:
  IDBFileHandle(IDBMutableFile* aMutableFile, FileMode aMode);
  ~IDBFileHandle();

  void SendFinish();

  void SendAbort();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_idbfilehandle_h__
