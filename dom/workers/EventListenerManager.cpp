/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventListenerManager.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Vector.h"
#include "js/GCAPI.h"
#include "mozilla/Util.h"
#include "nsAutoJSValHolder.h"

#include "Events.h"
#include "EventTarget.h"

using namespace mozilla::dom;
using namespace mozilla;
USING_WORKERS_NAMESPACE

struct ListenerData;

struct EventListenerManager::ListenerCollection :
  public LinkedListElement<EventListenerManager::ListenerCollection>
{
  jsid mTypeId;
  LinkedList<ListenerData> mListeners;

  static ListenerCollection*
  Add(JSContext* aCx, LinkedList<ListenerCollection>& aCollections, jsid aTypeId)
  {
    ListenerCollection* collection =
      static_cast<ListenerCollection*>(JS_malloc(aCx,
                                                 sizeof(ListenerCollection)));
    if (!collection) {
      return NULL;
    }

    new (collection) ListenerCollection(aTypeId);
    aCollections.insertBack(collection);

    return collection;
  }

  static void
  Remove(JSContext* aCx, ListenerCollection* aCollection)
  {
    aCollection->remove();
    MOZ_ASSERT(aCollection->mListeners.isEmpty());
    JS_free(aCx, aCollection);
  }

private:
  ListenerCollection(jsid aTypeId)
    : mTypeId(aTypeId)
  {
  }
};

struct ListenerData : LinkedListElement<ListenerData>
{
  JSObject* mListener;
  EventListenerManager::Phase mPhase;
  bool mWantsUntrusted;

  static ListenerData*
  Add(JSContext* aCx, LinkedList<ListenerData>& aListeners, JSObject* aListener,
      EventListenerManager::Phase aPhase, bool aWantsUntrusted)
  {
    ListenerData* listenerData =
      static_cast<ListenerData*>(JS_malloc(aCx, sizeof(ListenerData)));
    if (!listenerData) {
      return NULL;
    }

    new (listenerData) ListenerData(aListener, aPhase, aWantsUntrusted);
    aListeners.insertBack(listenerData);
    return listenerData;
  }

  static void
  Remove(JSContext* aCx, ListenerData* aListenerData)
  {
    if (JS::IsIncrementalBarrierNeeded(aCx)) {
      JS::IncrementalObjectBarrier(aListenerData->mListener);
    }

    aListenerData->remove();
    JS_free(aCx, aListenerData);
  }

private:
  ListenerData(JSObject* aListener, EventListenerManager::Phase aPhase,
               bool aWantsUntrusted)
    : mListener(aListener),
      mPhase(aPhase),
      mWantsUntrusted(aWantsUntrusted)
  {}
};

namespace {

template<typename T>
inline void
DestroyList(JSFreeOp* aFop, LinkedList<T>& aList)
{
  while (!aList.isEmpty()) {
    T* elem = aList.popFirst();
    JS_freeop(aFop, elem);
  }
}

inline EventListenerManager::ListenerCollection*
GetCollectionForType(const LinkedList<EventListenerManager::ListenerCollection>& aList,
                     const jsid& aTypeId)
{
  for (const EventListenerManager::ListenerCollection* collection = aList.getFirst();
       collection;
       collection = collection->getNext()) {
    if (collection->mTypeId == aTypeId) {
      // We need to either cast away const here or write a second copy of this
      // method that takes a non-const LinkedList
      return const_cast<EventListenerManager::ListenerCollection*>(collection);
    }
  }
  return nullptr;
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
  MOZ_ASSERT(mCollections.isEmpty());
}
#endif

void
EventListenerManager::TraceInternal(JSTracer* aTrc) const
{
  MOZ_ASSERT(!mCollections.isEmpty());

  for (const ListenerCollection* collection = mCollections.getFirst();
       collection;
       collection = collection->getNext()) {

    for (const ListenerData* listenerElem = collection->mListeners.getFirst();
         listenerElem;
         listenerElem = listenerElem->getNext()) {
      JS_CALL_OBJECT_TRACER(aTrc,
                            listenerElem->mListener,
                            "EventListenerManager listener object");
    }
  }
}

void
EventListenerManager::FinalizeInternal(JSFreeOp* aFop)
{
  MOZ_ASSERT(!mCollections.isEmpty());

  for (ListenerCollection* collection = mCollections.getFirst();
       collection;
       collection = collection->getNext()) {
    DestroyList(aFop, collection->mListeners);
  }

  DestroyList(aFop, mCollections);

  MOZ_ASSERT(mCollections.isEmpty());
}

void
EventListenerManager::Add(JSContext* aCx, const jsid& aType,
                          JSObject* aListener, Phase aPhase,
                          bool aWantsUntrusted, ErrorResult& aRv)
{
  MOZ_ASSERT(aListener);

  ListenerCollection* collection =
    GetCollectionForType(mCollections, aType);
  if (!collection) {
    collection = ListenerCollection::Add(aCx, mCollections, aType);
    if (!collection) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  for (ListenerData* listenerData = collection->mListeners.getFirst();
       listenerData;
       listenerData = listenerData->getNext()) {
    if (listenerData->mListener == aListener &&
        listenerData->mPhase == aPhase) {
      return;
    }
  }

  ListenerData* listenerData =
    ListenerData::Add(aCx, collection->mListeners,
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
    GetCollectionForType(mCollections, aType);
  if (collection) {
    for (ListenerData* listenerData = collection->mListeners.getFirst();
         listenerData;
         listenerData = listenerData->getNext()) {
      if (listenerData->mListener == aListener &&
          listenerData->mPhase == aPhase) {
        ListenerData::Remove(aCx, listenerData);
        if (aClearEmpty && collection->mListeners.isEmpty()) {
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
  const ListenerCollection* collection =
    GetCollectionForType(mCollections, aType);
  if (collection) {
    for (const ListenerData* listenerData = collection->mListeners.getFirst();
         listenerData;
         listenerData = listenerData->getNext()) {
        if (listenerData->mPhase == Onfoo) {
          return listenerData->mListener;
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

  if (mCollections.isEmpty()) {
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
    GetCollectionForType(mCollections, INTERNED_STRING_TO_JSID(aCx, eventType));
  if (!collection) {
    return false;
  }

  ContextAllocPolicy ap(aCx);

  // XXXbent There is no reason to use nsAutoJSValHolder here as we should be
  //         able to use js::AutoValueVector. Worse, nsAutoJSValHolder is much
  //         slower. However, js::AutoValueVector causes crashes on Android at
  //         the moment so we don't have much choice.
  js::Vector<nsAutoJSValHolder, 10, ContextAllocPolicy> listeners(ap);

  for (ListenerData* listenerData = collection->mListeners.getFirst();
       listenerData;
       listenerData = listenerData->getNext()) {
    // Listeners that don't want untrusted events will be skipped if this is an
    // untrusted event.
    if (eventIsTrusted || listenerData->mWantsUntrusted) {
      nsAutoJSValHolder holder;
      if (!holder.Hold(aCx)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return false;
      }

      holder = listenerData->mListener;

      if (!listeners.append(holder)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return false;
      }
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

    jsval listenerVal = listeners[index];

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
  MOZ_ASSERT(!mCollections.isEmpty());
  return !!GetCollectionForType(mCollections, aType);
}
