/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FileIOObject_h__
#define FileIOObject_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "nsIFile.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"

#include "mozilla/dom/DOMError.h"

#define NS_PROGRESS_EVENT_INTERVAL 50

namespace mozilla {

class ErrorResult;

namespace dom {

extern const uint64_t kUnknownSize;

// A common base class for FileReader and FileSaver

class FileIOObject : public DOMEventTargetHelper,
                     public nsIInputStreamCallback,
                     public nsITimerCallback
{
public:
  FileIOObject();

  NS_DECL_ISUPPORTS_INHERITED

  // Common methods
  void Abort(ErrorResult& aRv);
  uint16_t ReadyState() const
  {
    return mReadyState;
  }
  DOMError* GetError() const
  {
    return mError;
  }

  NS_METHOD GetOnabort(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnabort(JSContext* aCx, JS::Handle<JS::Value> aValue);
  NS_METHOD GetOnerror(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnerror(JSContext* aCx, JS::Handle<JS::Value> aValue);
  NS_METHOD GetOnprogress(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);
  NS_METHOD SetOnprogress(JSContext* aCx, JS::Handle<JS::Value> aValue);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(progress)

  NS_DECL_NSITIMERCALLBACK

  NS_DECL_NSIINPUTSTREAMCALLBACK

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileIOObject, DOMEventTargetHelper)

protected:
  virtual ~FileIOObject();

  // Implemented by the derived class to do whatever it needs to do for abort
  virtual void DoAbort(nsAString& aEvent) = 0;

  virtual nsresult DoReadData(nsIAsyncInputStream* aStream, uint64_t aCount) = 0;

  virtual nsresult DoOnLoadEnd(nsresult aStatus, nsAString& aSuccessEvent,
                               nsAString& aTerminationEvent) = 0;

  nsresult OnLoadEnd(nsresult aStatus);
  nsresult DoAsyncWait(nsIAsyncInputStream* aStream);

  void StartProgressEventTimer();
  void ClearProgressEventTimer();
  void DispatchError(nsresult rv, nsAString& finalEvent);
  nsresult DispatchProgressEvent(const nsAString& aType);

  nsCOMPtr<nsITimer> mProgressNotifier;
  bool mProgressEventWasDelayed;
  bool mTimerIsActive;

  nsCOMPtr<nsIAsyncInputStream> mAsyncStream;

  nsRefPtr<DOMError> mError;

  uint16_t mReadyState;

  uint64_t mTotal;
  uint64_t mTransferred;
};

} // namespace dom
} // namespace mozilla

#endif
