/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventListenerManager.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Vector.h"
#include "mozilla/Util.h"

#include "Events.h"
#include "EventTarget.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;

namespace {

struct ListenerCollection : PRCList
{
  jsid mTypeId;
  PRCList mListenerHead;

  static ListenerCollection*
  Add(JSContext* aCx, ListenerCollection* aCollectionHead, jsid aTypeId)
  {
    ListenerCollection* collection =
      static_cast<ListenerCollection*>(JS_malloc(aCx,
                                                 sizeof(ListenerCollection)));
    if (!collection) {
      return NULL;
    }

    PR_APPEND_LINK(collection, aCollectionHead);

    collection->mTypeId = aTypeId;
    PR_INIT_CLIST(&collection->mListenerHead);
    return collection;
  }

  static void
  Remove(JSContext* aCx, ListenerCollection* aCollection)
  {
    PR_REMOVE_LINK(aCollection);
    JS_free(aCx, aCollection);
  }
};

struct ListenerData : PRCList
{
  JSObject* mListener;
  EventListenerManager::Phase mPhase;
  bool mWantsUntrusted;

  static ListenerData*
  Add(JSContext* aCx, ListenerData* aListenerDataHead, JSObject* aListener,
      EventListenerManager::Phase aPhase, bool aWantsUntrusted)
  {
    ListenerData* listenerData =
      static_cast<ListenerData*>(JS_malloc(aCx, sizeof(ListenerData)));
    if (!listenerData) {
      return NULL;
    }

    PR_APPEND_LINK(listenerData, aListenerDataHead);

    listenerData->mListener = aListener;
    listenerData->mPhase = aPhase;
    listenerData->mWantsUntrusted = aWantsUntrusted;
    return listenerData;
  }

  static void
  Remove(JSContext* aCx, ListenerData* aListenerData)
  {
    if (js::IsIncrementalBarrierNeeded(aCx)) {
      js:: IncrementalReferenceBarrier(aListenerData->mListener);
  }

    PR_REMOVE_LINK(aListenerData);
    JS_free(aCx, aListenerData);
  }
};

inline void
DestroyList(JSFreeOp* aFop, PRCList* aListHead)
{
  for (PRCList* elem = PR_NEXT_LINK(aListHead); elem != aListHead; ) {
    PRCList* nextElem = PR_NEXT_LINK(elem);
    JS_freeop(aFop, elem);
    elem = nextElem;
  }
}

inline ListenerCollection*
GetCollectionForType(const PRCList* aHead, const jsid& aTypeId)
{
  for (PRCList* elem = PR_NEXT_LINK(aHead);
       elem != aHead;
       elem = PR_NEXT_LINK(elem)) {
    ListenerCollection* collection = static_cast<ListenerCollection*>(elem);
    if (collection->mTypeId == aTypeId) {
      return collection;
    }
  }
  return NULL;
}

class ContextAllocPolicy
{
  JSContext* const mCx;

public:
  ContextAllocPolicy(JSContext* aCx)
  : mCx(aCx)
  { }

  void*
  malloc_(size_t aBytes) const
  {
    JSAutoRequest ar(mCx);
    return JS_malloc(mCx, aBytes);
  }

  void*
  realloc_(void* aPtr, size_t aOldBytes, size_t aBytes) const
  {
    JSAutoRequest ar(mCx);
    return JS_realloc(mCx, aPtr, aBytes);
  }

  void
  free_(void* aPtr) const
  {
    JS_free(mCx, aPtr);
  }

  void
  reportAllocOverflow() const
  {
    JS_ReportAllocationOverflow(mCx);
  }
};

} // anonymous namespace

#ifdef DEBUG
EventListenerManager::~EventListenerManager()
{
  MOZ_ASSERT(PR_CLIST_IS_EMPTY(&mCollectionHead));
}
#endif

void
EventListenerManager::TraceInternal(JSTracer* aTrc) const
{
  MOZ_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));

  for (PRCList* collectionElem = PR_NEXT_LINK(&mCollectionHead);
       collectionElem != &mCollectionHead;
       collectionElem = PR_NEXT_LINK(collectionElem)) {
    ListenerCollection* collection =
      static_cast<ListenerCollection*>(collectionElem);

    for (PRCList* listenerElem = PR_NEXT_LINK(&collection->mListenerHead);
         listenerElem != &collection->mListenerHead;
         listenerElem = PR_NEXT_LINK(listenerElem)) {
      JS_CALL_OBJECT_TRACER(aTrc,
                            static_cast<ListenerData*>(listenerElem)->mListener,
                            "EventListenerManager listener object");
    }
  }
}

void
EventListenerManager::FinalizeInternal(JSFreeOp* aFop)
{
  MOZ_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));

  for (PRCList* elem = PR_NEXT_LINK(&mCollectionHead);
       elem != &mCollectionHead;
       elem = PR_NEXT_LINK(elem)) {
    DestroyList(aFop, &static_cast<ListenerCollection*>(elem)->mListenerHead);
  }

  DestroyList(aFop, &mCollectionHead);

#ifdef DEBUG
  PR_INIT_CLIST(&mCollectionHead);
#endif
}

void
EventListenerManager::Add(JSContext* aCx, const jsid& aType,
                          JSObject* aListener, Phase aPhase,
                          bool aWantsUntrusted, ErrorResult& aRv)
{
  MOZ_ASSERT(aListener);

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead, aType);
  if (!collection) {
    ListenerCollection* head =
      static_cast<ListenerCollection*>(&mCollectionHead);
    collection = ListenerCollection::Add(aCx, head, aType);
    if (!collection) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
    ListenerData* listenerData = static_cast<ListenerData*>(elem);
    if (listenerData->mListener == aListener &&
        listenerData->mPhase == aPhase) {
      return;
    }
  }

  ListenerData* listenerData =
    ListenerData::Add(aCx,
                      static_cast<ListenerData*>(&collection->mListenerHead),
                      aListener, aPhase, aWantsUntrusted);
  if (!listenerData) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
}

void
EventListenerManager::Remove(JSContext* aCx, const jsid& aType,
                             JSObject* aListener, Phase aPhase,
                             bool aClearEmpty)
{
  MOZ_ASSERT(aListener);

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead, aType);
  if (collection) {
  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
      ListenerData* listenerData = static_cast<ListenerData*>(elem);
      if (listenerData->mListener == aListener &&
          listenerData->mPhase == aPhase) {
        ListenerData::Remove(aCx, listenerData);
      if (aClearEmpty && PR_CLIST_IS_EMPTY(&collection->mListenerHead)) {
        ListenerCollection::Remove(aCx, collection);
      }
      break;
    }
  }
  }
}

JSObject*
EventListenerManager::GetEventListener(const jsid& aType) const
{
  if (!PR_CLIST_IS_EMPTY(&mCollectionHead)) {
    const ListenerCollection* collection =
      GetCollectionForType(&mCollectionHead, aType);
  if (collection) {
    for (PRCList* elem = PR_PREV_LINK(&collection->mListenerHead);
         elem != &collection->mListenerHead;
         elem = PR_NEXT_LINK(elem)) {
        ListenerData* listenerData = static_cast<ListenerData*>(elem);
        if (listenerData->mPhase == Onfoo) {
          return listenerData->mListener;
      }
    }
  }
    }

  return NULL;
}

bool
EventListenerManager::DispatchEvent(JSContext* aCx, const EventTarget& aTarget,
                                    JSObject* aEvent, ErrorResult& aRv) const
{
  using namespace mozilla::dom::workers::events;

  if (!IsSupportedEventClass(aEvent)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  jsval val;
  if (!JS_GetProperty(aCx, aEvent, "target", &val)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (!JSVAL_IS_NULL(val)) {
    // Already has a target, must be recursively dispatched. Throw.
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (PR_CLIST_IS_EMPTY(&mCollectionHead)) {
    return false;
  }

  JSString* eventType;
  JSBool eventIsTrusted;

  if (!JS_GetProperty(aCx, aEvent, "type", &val) ||
      !(eventType = JS_ValueToString(aCx, val)) ||
      !(eventType = JS_InternJSString(aCx, eventType))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  // We have already ensure that the event is one of our types of events so
  // there is no need to worry about this property being faked.
  if (!JS_GetProperty(aCx, aEvent, "isTrusted", &val) ||
      !JS_ValueToBoolean(aCx, val, &eventIsTrusted)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead,
                         INTERNED_STRING_TO_JSID(aCx, eventType));
  if (!collection) {
    return false;
  }

  ContextAllocPolicy ap(aCx);
  js::Vector<JSObject*, 10, ContextAllocPolicy> listeners(ap);

  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
    ListenerData* listenerData = static_cast<ListenerData*>(elem);

    // Listeners that don't want untrusted events will be skipped if this is an
    // untrusted event.
    if ((eventIsTrusted || listenerData->mWantsUntrusted) &&
        !listeners.append(listenerData->mListener)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (listeners.empty()) {
    return false;
  }

  SetEventTarget(aEvent, aTarget.GetJSObject());

  for (size_t index = 0; index < listeners.length(); index++) {
    if (EventImmediatePropagationStopped(aEvent)) {
      break;
    }

    // If anything fails in here we want to report the exception and continue on
    // to the next listener rather than bailing out. If something fails and
    // does not set an exception then we bail out entirely as we've either run
    // out of memory or the operation callback has indicated that we should
    // stop running.

    jsval listenerVal = OBJECT_TO_JSVAL(listeners[index]);

    JSObject* listenerObj;
    if (!JS_ValueToObject(aCx, listenerVal, &listenerObj)) {
      if (!JS_ReportPendingException(aCx)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      continue;
    }

    static const char sHandleEventChars[] = "handleEvent";

    JSObject* thisObj = aTarget.GetJSObject();

    JSBool hasHandleEvent;
    if (!JS_HasProperty(aCx, listenerObj, sHandleEventChars, &hasHandleEvent)) {
      if (!JS_ReportPendingException(aCx)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      continue;
    }

    if (hasHandleEvent) {
      if (!JS_GetProperty(aCx, listenerObj, sHandleEventChars, &listenerVal)) {
        if (!JS_ReportPendingException(aCx)) {
          aRv.Throw(NS_ERROR_FAILURE);
          return false;
        }
        continue;
      }

      thisObj = listenerObj;
    }

    jsval argv[] = { OBJECT_TO_JSVAL(aEvent) };
    jsval rval = JSVAL_VOID;
    if (!JS_CallFunctionValue(aCx, thisObj, listenerVal, ArrayLength(argv),
                              argv, &rval)) {
      if (!JS_ReportPendingException(aCx)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      continue;
    }
  }

  return EventWasCanceled(aEvent);
}

bool
EventListenerManager::HasListenersForTypeInternal(JSContext* aCx,
                                                  const jsid& aType) const
{
  MOZ_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));
  return !!GetCollectionForType(&mCollectionHead, aType);
}
