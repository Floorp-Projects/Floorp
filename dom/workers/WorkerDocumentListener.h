/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkerDocumentListener_h__
#define mozilla_dom_WorkerDocumentListener_h__

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
class ThreadSafeWorkerRef;
class WorkerPrivate;
class WeakWorkerRef;

class WorkerDocumentListener final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerDocumentListener)

 public:
  WorkerDocumentListener();

  void OnVisible(bool aVisible);
  void SetListening(uint64_t aWindowID, bool aListen);
  void Destroy();

  static RefPtr<WorkerDocumentListener> Create(WorkerPrivate* aWorkerPrivate);

 private:
  ~WorkerDocumentListener();

  Mutex mMutex MOZ_UNANNOTATED;  // protects mWorkerRef
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_WorkerDocumentListener_h__ */
