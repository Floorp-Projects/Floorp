/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_listenermanager_h__
#define mozilla_dom_workers_listenermanager_h__

#include "mozilla/dom/workers/Workers.h"

#include "prclist.h"

#include "mozilla/ErrorResult.h"

BEGIN_WORKERS_NAMESPACE

class EventTarget;

// XXX Current impl doesn't divide into phases.
// XXX Current impl doesn't handle event target chains.
class EventListenerManager
{
  PRCList mCollectionHead;

public:
  EventListenerManager()
  {
    PR_INIT_CLIST(&mCollectionHead);
  }

#ifdef DEBUG
  ~EventListenerManager();
#endif

  void
  _trace(JSTracer* aTrc) const
  {
    if (!PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      TraceInternal(aTrc);
    }
  }

  void
  _finalize(JSFreeOp* aFop)
  {
    if (!PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      FinalizeInternal(aFop);
    }
  }

  enum Phase
  {
    All = 0,
    Capturing,
    Onfoo,
    Bubbling
  };

  void
  AddEventListener(JSContext* aCx, const jsid& aType, JSObject* aListener,
                   bool aCapturing, bool aWantsUntrusted, ErrorResult& aRv)
  {
    Add(aCx, aType, aListener, aCapturing ? Capturing : Bubbling,
        aWantsUntrusted, aRv);
  }

  void
  RemoveEventListener(JSContext* aCx, const jsid& aType, JSObject* aListener,
                      bool aCapturing)
  {
    if (PR_CLIST_IS_EMPTY(&mCollectionHead)) {
      return;
    }
    Remove(aCx, aType, aListener, aCapturing ? Capturing : Bubbling, true);
  }

  bool
  DispatchEvent(JSContext* aCx, const EventTarget& aTarget, JSObject* aEvent,
                ErrorResult& aRv) const;

  JSObject*
  GetEventListener(const jsid& aType) const;

  void
  SetEventListener(JSContext* aCx, const jsid& aType, JSObject* aListener,
                   ErrorResult& aRv)
  {
    JSObject* existing = GetEventListener(aType);
    if (existing) {
      Remove(aCx, aType, existing, Onfoo, false);
    }

    if (aListener) {
      Add(aCx, aType, aListener, Onfoo, false, aRv);
    }
  }

  bool
  HasListeners() const
  {
    return !PR_CLIST_IS_EMPTY(&mCollectionHead);
  }

  bool
  HasListenersForType(JSContext* aCx, const jsid& aType) const
  {
    return HasListeners() && HasListenersForTypeInternal(aCx, aType);
  }

private:
  void
  TraceInternal(JSTracer* aTrc) const;

  void
  FinalizeInternal(JSFreeOp* aFop);

  void
  Add(JSContext* aCx, const jsid& aType, JSObject* aListener, Phase aPhase,
      bool aWantsUntrusted, ErrorResult& aRv);

  void
  Remove(JSContext* aCx, const jsid& aType, JSObject* aListener, Phase aPhase,
         bool aClearEmpty);

  bool
  HasListenersForTypeInternal(JSContext* aCx, const jsid& aType) const;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_listenermanager_h__ */
