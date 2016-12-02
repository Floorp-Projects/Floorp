/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileReader_h
#define mozilla_dom_FileReader_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"

#include "nsIAsyncInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "WorkerHolder.h"

#define NS_PROGRESS_EVENT_INTERVAL 50

class nsITimer;
class nsIEventTarget;

namespace mozilla {
namespace dom {

class Blob;
class DOMError;

namespace workers {
class WorkerPrivate;
}

extern const uint64_t kUnknownSize;

class FileReaderDecreaseBusyCounter;

class FileReader final : public DOMEventTargetHelper,
                         public nsIInterfaceRequestor,
                         public nsSupportsWeakReference,
                         public nsIInputStreamCallback,
                         public nsITimerCallback,
                         public workers::WorkerHolder
{
  friend class FileReaderDecreaseBusyCounter;

public:
  FileReader(nsIGlobalObject* aGlobal,
             workers::WorkerPrivate* aWorkerPrivate);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(FileReader,
                                                         DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  static already_AddRefed<FileReader>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);
  void ReadAsArrayBuffer(JSContext* aCx, Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_ARRAYBUFFER, aRv);
  }

  void ReadAsText(Blob& aBlob, const nsAString& aLabel, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, aLabel, FILE_AS_TEXT, aRv);
  }

  void ReadAsDataURL(Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_DATAURL, aRv);
  }

  void Abort();

  uint16_t ReadyState() const
  {
    return static_cast<uint16_t>(mReadyState);
  }

  DOMError* GetError() const
  {
    return mError;
  }

  void GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                 ErrorResult& aRv);

  IMPL_EVENT_HANDLER(loadstart)
  IMPL_EVENT_HANDLER(progress)
  IMPL_EVENT_HANDLER(load)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(loadend)

  void ReadAsBinaryString(Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_BINARY, aRv);
  }

  // WorkerHolder
  bool Notify(workers::Status) override;

private:
  virtual ~FileReader();

  // This must be in sync with dom/webidl/FileReader.webidl
  enum eReadyState {
    EMPTY = 0,
    LOADING = 1,
    DONE = 2
  };

  enum eDataFormat {
    FILE_AS_ARRAYBUFFER,
    FILE_AS_BINARY,
    FILE_AS_TEXT,
    FILE_AS_DATAURL
  };

  void RootResultArrayBuffer();

  void ReadFileContent(Blob& aBlob,
                       const nsAString &aCharset, eDataFormat aDataFormat,
                       ErrorResult& aRv);
  nsresult GetAsText(Blob *aBlob, const nsACString &aCharset,
                     const char *aFileData, uint32_t aDataLen,
                     nsAString &aResult);
  nsresult GetAsDataURL(Blob *aBlob, const char *aFileData,
                        uint32_t aDataLen, nsAString &aResult);

  nsresult OnLoadEnd(nsresult aStatus);

  void StartProgressEventTimer();
  void ClearProgressEventTimer();
  void DispatchError(nsresult rv, nsAString& finalEvent);
  nsresult DispatchProgressEvent(const nsAString& aType);

  nsresult DoAsyncWait();
  nsresult DoReadData(uint64_t aCount);

  nsresult DoOnLoadEnd(nsresult aStatus, nsAString& aSuccessEvent,
                       nsAString& aTerminationEvent);

  void FreeFileData()
  {
    free(mFileData);
    mFileData = nullptr;
    mDataLen = 0;
  }

  nsresult IncreaseBusyCounter();
  void DecreaseBusyCounter();

  void Shutdown();

  char *mFileData;
  RefPtr<Blob> mBlob;
  nsCString mCharset;
  uint32_t mDataLen;

  eDataFormat mDataFormat;

  nsString mResult;

  JS::Heap<JSObject*> mResultArrayBuffer;

  nsCOMPtr<nsITimer> mProgressNotifier;
  bool mProgressEventWasDelayed;
  bool mTimerIsActive;

  nsCOMPtr<nsIAsyncInputStream> mAsyncStream;

  RefPtr<DOMError> mError;

  eReadyState mReadyState;

  uint64_t mTotal;
  uint64_t mTransferred;

  nsCOMPtr<nsIEventTarget> mTarget;

  uint64_t mBusyCount;

  // Kept alive with a WorkerHolder.
  workers::WorkerPrivate* mWorkerPrivate;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FileReader_h
