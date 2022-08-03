/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkerCSPEventListener_h
#define mozilla_dom_WorkerCSPEventListener_h

#include "mozilla/dom/WorkerRef.h"
#include "mozilla/Mutex.h"
#include "nsIContentSecurityPolicy.h"

namespace mozilla::dom {

class WeakWorkerRef;
class WorkerRef;
class WorkerPrivate;

class WorkerCSPEventListener final : public nsICSPEventListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICSPEVENTLISTENER

  static already_AddRefed<WorkerCSPEventListener> Create(
      WorkerPrivate* aWorkerPrivate);

 private:
  WorkerCSPEventListener();
  ~WorkerCSPEventListener() = default;

  Mutex mMutex;

  // Protected by mutex.
  RefPtr<WeakWorkerRef> mWorkerRef MOZ_GUARDED_BY(mMutex);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WorkerCSPEventListener_h
