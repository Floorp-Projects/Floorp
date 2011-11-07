/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "ListenerManager.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "js/Vector.h"

#include "Events.h"

using namespace mozilla;
using dom::workers::events::ListenerManager;

namespace {

struct Listener;

struct ListenerCollection : PRCList
{
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

  jsid mTypeId;
  PRCList mListenerHead;
};

struct Listener : PRCList
{
  static Listener*
  Add(JSContext* aCx, Listener* aListenerHead, jsval aListenerVal,
      ListenerManager::Phase aPhase, bool aWantsUntrusted)
  {
    Listener* listener =
      static_cast<Listener*>(JS_malloc(aCx, sizeof(Listener)));
    if (!listener) {
      return NULL;
    }

    PR_APPEND_LINK(listener, aListenerHead);

    listener->mListenerVal = aListenerVal;
    listener->mPhase = aPhase;
    listener->mWantsUntrusted = aWantsUntrusted;
    return listener;
  }

  static void
  Remove(JSContext* aCx, Listener* aListener)
  {
    PR_REMOVE_LINK(aListener);
    JS_free(aCx, aListener);
  }

  jsval mListenerVal;
  ListenerManager::Phase mPhase;
  bool mWantsUntrusted;
};

void
DestroyList(JSContext* aCx, PRCList* aListHead)
{
  for (PRCList* elem = PR_NEXT_LINK(aListHead); elem != aListHead; ) {
    PRCList* nextElem = PR_NEXT_LINK(elem);
    JS_free(aCx, elem);
    elem = nextElem;
  }
}

ListenerCollection*
GetCollectionForType(PRCList* aHead, jsid aTypeId)
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

} // anonymous namespace

#ifdef DEBUG
ListenerManager::~ListenerManager()
{
  JS_ASSERT(PR_CLIST_IS_EMPTY(&mCollectionHead));
}
#endif

void
ListenerManager::TraceInternal(JSTracer* aTrc)
{
  JS_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));

  for (PRCList* collectionElem = PR_NEXT_LINK(&mCollectionHead);
       collectionElem != &mCollectionHead;
       collectionElem = PR_NEXT_LINK(collectionElem)) {
    ListenerCollection* collection =
      static_cast<ListenerCollection*>(collectionElem);

    for (PRCList* listenerElem = PR_NEXT_LINK(&collection->mListenerHead);
         listenerElem != &collection->mListenerHead;
         listenerElem = PR_NEXT_LINK(listenerElem)) {
      JS_CALL_VALUE_TRACER(aTrc,
                           static_cast<Listener*>(listenerElem)->mListenerVal,
                           "EventListenerManager listener value");
    }
  }
}

void
ListenerManager::FinalizeInternal(JSContext* aCx)
{
  JS_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));

  for (PRCList* elem = PR_NEXT_LINK(&mCollectionHead);
       elem != &mCollectionHead;
       elem = PR_NEXT_LINK(elem)) {
    DestroyList(aCx, &static_cast<ListenerCollection*>(elem)->mListenerHead);
  }

  DestroyList(aCx, &mCollectionHead);

#ifdef DEBUG
  PR_INIT_CLIST(&mCollectionHead);
#endif
}

bool
ListenerManager::Add(JSContext* aCx, JSString* aType, jsval aListenerVal,
                     Phase aPhase, bool aWantsUntrusted)
{
  aType = JS_InternJSString(aCx, aType);
  if (!aType) {
    return false;
  }

  if (JSVAL_IS_PRIMITIVE(aListenerVal)) {
    JS_ReportError(aCx, "Bad listener!");
    return false;
  }

  jsid typeId = INTERNED_STRING_TO_JSID(aCx, aType);

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead, typeId);
  if (!collection) {
    ListenerCollection* head =
      static_cast<ListenerCollection*>(&mCollectionHead);
    collection = ListenerCollection::Add(aCx, head, typeId);
    if (!collection) {
      return false;
    }
  }

  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
    Listener* listener = static_cast<Listener*>(elem);
    if (listener->mListenerVal == aListenerVal && listener->mPhase == aPhase) {
      return true;
    }
  }

  Listener* listener =
    Listener::Add(aCx, static_cast<Listener*>(&collection->mListenerHead),
                  aListenerVal, aPhase, aWantsUntrusted);
  if (!listener) {
    return false;
  }

  return true;
}

bool
ListenerManager::Remove(JSContext* aCx, JSString* aType, jsval aListenerVal,
                        Phase aPhase, bool aClearEmpty)
{
  aType = JS_InternJSString(aCx, aType);
  if (!aType) {
    return false;
  }

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead, INTERNED_STRING_TO_JSID(aCx, aType));
  if (!collection) {
    return true;
  }

  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
    Listener* listener = static_cast<Listener*>(elem);
    if (listener->mListenerVal == aListenerVal && listener->mPhase == aPhase) {
      Listener::Remove(aCx, listener);
      if (aClearEmpty && PR_CLIST_IS_EMPTY(&collection->mListenerHead)) {
        ListenerCollection::Remove(aCx, collection);
      }
      break;
    }
  }

  return true;
}

bool
ListenerManager::SetEventListener(JSContext* aCx, JSString* aType,
                                  jsval aListener)
{
  jsval existing;
  if (!GetEventListener(aCx, aType, &existing)) {
    return false;
  }

  if (!JSVAL_IS_VOID(existing) &&
      !Remove(aCx, aType, existing, Onfoo, false)) {
    return false;
  }

  return JSVAL_IS_VOID(aListener) || JSVAL_IS_NULL(aListener) ?
         true :
         Add(aCx, aType, aListener, Onfoo, false);
}

bool
ListenerManager::GetEventListener(JSContext* aCx, JSString* aType,
                                  jsval* aListenerVal)
{
  if (PR_CLIST_IS_EMPTY(&mCollectionHead)) {
    *aListenerVal = JSVAL_VOID;
    return true;
  }

  aType = JS_InternJSString(aCx, aType);
  if (!aType) {
    return false;
  }

  jsid typeId = INTERNED_STRING_TO_JSID(aCx, aType);

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead, typeId);
  if (collection) {
    for (PRCList* elem = PR_PREV_LINK(&collection->mListenerHead);
         elem != &collection->mListenerHead;
         elem = PR_NEXT_LINK(elem)) {
      Listener* listener = static_cast<Listener*>(elem);
      if (listener->mPhase == Onfoo) {
        *aListenerVal = listener->mListenerVal;
        return true;
      }
    }
  }
  *aListenerVal = JSVAL_VOID;
  return true;
}

bool
ListenerManager::DispatchEvent(JSContext* aCx, JSObject* aTarget,
                               JSObject* aEvent, bool* aPreventDefaultCalled)
{
  if (!events::IsSupportedEventClass(aCx, aEvent)) {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_METHOD,
                         "EventTarget", "dispatchEvent", "Event object");
    return false;
  }

  jsval val;
  if (!JS_GetProperty(aCx, aEvent, "target", &val)) {
    return false;
  }

  if (!JSVAL_IS_NULL(val)) {
    // Already has a target, must be recursively dispatched. Throw.
    JS_ReportError(aCx, "Cannot recursively dispatch the same event!");
    return false;
  }

  if (PR_CLIST_IS_EMPTY(&mCollectionHead)) {
    *aPreventDefaultCalled = false;
    return true;
  }

  JSString* eventType;
  JSBool eventIsTrusted;

  if (!JS_GetProperty(aCx, aEvent, "type", &val) ||
      !(eventType = JS_ValueToString(aCx, val)) ||
      !(eventType = JS_InternJSString(aCx, eventType))) {
    return false;
  }

  // We have already ensure that the event is one of our types of events so
  // there is no need to worry about this property being faked.
  if (!JS_GetProperty(aCx, aEvent, "isTrusted", &val) ||
      !JS_ValueToBoolean(aCx, val, &eventIsTrusted)) {
    return false;
  }

  ListenerCollection* collection =
    GetCollectionForType(&mCollectionHead,
                         INTERNED_STRING_TO_JSID(aCx, eventType));
  if (!collection) {
    *aPreventDefaultCalled = false;
    return true;
  }

  js::ContextAllocPolicy ap(aCx);
  js::Vector<jsval, 10, js::ContextAllocPolicy> listeners(ap);

  for (PRCList* elem = PR_NEXT_LINK(&collection->mListenerHead);
       elem != &collection->mListenerHead;
       elem = PR_NEXT_LINK(elem)) {
    Listener* listener = static_cast<Listener*>(elem);

    // Listeners that don't want untrusted events will be skipped if this is an
    // untrusted event.
    if ((eventIsTrusted || listener->mWantsUntrusted) &&
        !listeners.append(listener->mListenerVal)) {
      return false;
    }
  }

  if (listeners.empty()) {
    return true;
  }

  if (!events::SetEventTarget(aCx, aEvent, aTarget)) {
    return false;
  }

  for (size_t index = 0; index < listeners.length(); index++) {
    if (events::EventImmediatePropagationStopped(aCx, aEvent)) {
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
        return false;
      }
      continue;
    }

    static const char sHandleEventChars[] = "handleEvent";

    JSBool hasHandleEvent;
    if (!JS_HasProperty(aCx, listenerObj, sHandleEventChars, &hasHandleEvent)) {
      if (!JS_ReportPendingException(aCx)) {
        return false;
      }
      continue;
    }

    if (hasHandleEvent) {
      if (!JS_GetProperty(aCx, listenerObj, sHandleEventChars, &listenerVal)) {
        if (!JS_ReportPendingException(aCx)) {
          return false;
        }
        continue;
      }
    }

    jsval argv[] = { OBJECT_TO_JSVAL(aEvent) };
    jsval rval = JSVAL_VOID;
    if (!JS_CallFunctionValue(aCx, aTarget, listenerVal, ArrayLength(argv),
                              argv, &rval)) {
      if (!JS_ReportPendingException(aCx)) {
        return false;
      }
      continue;
    }
  }

  if (!events::SetEventTarget(aCx, aEvent, NULL)) {
    return false;
  }

  *aPreventDefaultCalled = events::EventWasCanceled(aCx, aEvent);
  return true;
}

bool
ListenerManager::HasListenersForTypeInternal(JSContext* aCx, JSString* aType)
{
  JS_ASSERT(!PR_CLIST_IS_EMPTY(&mCollectionHead));

  aType = JS_InternJSString(aCx, aType);
  if (!aType) {
    return false;
  }

  jsid typeId = INTERNED_STRING_TO_JSID(aCx, aType);
  return !!GetCollectionForType(&mCollectionHead, typeId);
}
