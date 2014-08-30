/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_performance_h__
#define mozilla_dom_workers_performance_h__

#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "Workers.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class Performance MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Performance)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Performance)

  Performance(WorkerPrivate* aWorkerPrivate);

private:
  ~Performance();

  WorkerPrivate* mWorkerPrivate;

public:
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsISupports*
  GetParentObject() const
  {
    // There's only one global on a worker, so we don't need to specify.
    return nullptr;
  }

  // WebIDL (public APIs)
  double Now() const;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_performance_h__
