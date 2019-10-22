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
#include "nsINamed.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWeakReference.h"

#define NS_PROGRESS_EVENT_INTERVAL 50

class nsITimer;
class nsIEventTarget;

namespace mozilla {
namespace dom {

class Blob;
class DOMException;
class StrongWorkerRef;
class WeakWorkerRef;

extern const uint64_t kUnknownSize;

class FileReaderDecreaseBusyCounter;

// 26a79031-c94b-47e9-850a-f04fe17bc026
#define FILEREADER_ID                                \
  {                                                  \
    0x26a79031, 0xc94b, 0x47e9, {                    \
      0x85, 0x0a, 0xf0, 0x4f, 0xe1, 0x7b, 0xc0, 0x26 \
    }                                                \
  }

class FileReader final : public DOMEventTargetHelper,
                         public nsIInterfaceRequestor,
                         public nsSupportsWeakReference,
                         public nsIInputStreamCallback,
                         public nsITimerCallback,
                         public nsINamed {
  friend class FileReaderDecreaseBusyCounter;

 public:
  FileReader(nsIGlobalObject* aGlobal, WeakWorkerRef* aWorkerRef);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSINAMED

  NS_DECLARE_STATIC_IID_ACCESSOR(FILEREADER_ID)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(FileReader,
                                                         DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  static already_AddRefed<FileReader> Constructor(const GlobalObject& aGlobal);
  void ReadAsArrayBuffer(JSContext* aCx, Blob& aBlob, ErrorResult& aRv) {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_ARRAYBUFFER, aRv);
  }

  void ReadAsText(Blob& aBlob, const Optional<nsAString>& aLabel,
                  ErrorResult& aRv) {
    if (aLabel.WasPassed()) {
      ReadFileContent(aBlob, aLabel.Value(), FILE_AS_TEXT, aRv);
    } else {
      ReadFileContent(aBlob, EmptyString(), FILE_AS_TEXT, aRv);
    }
  }

  void ReadAsDataURL(Blob& aBlob, ErrorResult& aRv) {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_DATAURL, aRv);
  }

  void Abort();

  uint16_t ReadyState() const { return static_cast<uint16_t>(mReadyState); }

  DOMException* GetError() const { return mError; }

  void GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                 ErrorResult& aRv);

  IMPL_EVENT_HANDLER(loadstart)
  IMPL_EVENT_HANDLER(progress)
  IMPL_EVENT_HANDLER(load)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(loadend)

  void ReadAsBinaryString(Blob& aBlob, ErrorResult& aRv) {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_BINARY, aRv);
  }

  enum eDataFormat {
    FILE_AS_ARRAYBUFFER,
    FILE_AS_BINARY,
    FILE_AS_TEXT,
    FILE_AS_DATAURL
  };

  eDataFormat DataFormat() const { return mDataFormat; }
  const nsString& Result() const { return mResult; }

 private:
  virtual ~FileReader();

  // This must be in sync with dom/webidl/FileReader.webidl
  enum eReadyState { EMPTY = 0, LOADING = 1, DONE = 2 };

  void RootResultArrayBuffer();

  void ReadFileContent(Blob& aBlob, const nsAString& aCharset,
                       eDataFormat aDataFormat, ErrorResult& aRv);
  nsresult GetAsText(Blob* aBlob, const nsACString& aCharset,
                     const char* aFileData, uint32_t aDataLen,
                     nsAString& aResult);
  nsresult GetAsDataURL(Blob* aBlob, const char* aFileData, uint32_t aDataLen,
                        nsAString& aResult);

  nsresult OnLoadEnd(nsresult aStatus);

  void StartProgressEventTimer();
  void ClearProgressEventTimer();

  void FreeDataAndDispatchSuccess();
  void FreeDataAndDispatchError();
  void FreeDataAndDispatchError(nsresult aRv);
  nsresult DispatchProgressEvent(const nsAString& aType);

  nsresult DoAsyncWait();
  nsresult DoReadData(uint64_t aCount);

  void OnLoadEndArrayBuffer();

  void FreeFileData();

  nsresult IncreaseBusyCounter();
  void DecreaseBusyCounter();

  void Shutdown();

  char* mFileData;
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

  RefPtr<DOMException> mError;

  eReadyState mReadyState;

  uint64_t mTotal;
  uint64_t mTransferred;

  nsCOMPtr<nsIEventTarget> mTarget;

  uint64_t mBusyCount;

  // This is set if FileReader is created on workers, but it is null if the
  // worker is shutting down. The null value is checked in ReadFileContent()
  // before starting any reading.
  RefPtr<WeakWorkerRef> mWeakWorkerRef;

  // This value is set when the reading starts in order to keep the worker alive
  // during the process.
  RefPtr<StrongWorkerRef> mStrongWorkerRef;
};

NS_DEFINE_STATIC_IID_ACCESSOR(FileReader, FILEREADER_ID)

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileReader_h
