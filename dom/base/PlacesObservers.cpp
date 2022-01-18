/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlacesObservers.h"

#include "PlacesWeakCallbackWrapper.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla::dom {

template <class T>
struct Flagged {
  Flagged(uint32_t aFlags, T&& aValue)
      : flags(aFlags), value(std::forward<T>(aValue)) {}
  Flagged(Flagged&& aOther)
      : Flagged(std::move(aOther.flags), std::move(aOther.value)) {}
  Flagged(const Flagged& aOther) = default;
  ~Flagged() = default;

  uint32_t flags;
  T value;
};

template <class T>
using FlaggedArray = nsTArray<Flagged<T>>;

template <class T>
struct ListenerCollection {
  static StaticAutoPtr<FlaggedArray<T>> gListeners;
  static StaticAutoPtr<FlaggedArray<T>> gListenersToRemove;

  static FlaggedArray<T>* GetListeners(bool aDoNotInit = false) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gListeners && !aDoNotInit) {
      gListeners = new FlaggedArray<T>();
      ClearOnShutdown(&gListeners);
    }
    return gListeners;
  }

  static FlaggedArray<T>* GetListenersToRemove(bool aDoNotInit = false) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gListenersToRemove && !aDoNotInit) {
      gListenersToRemove = new FlaggedArray<T>();
      ClearOnShutdown(&gListenersToRemove);
    }
    return gListenersToRemove;
  }
};

template <class T>
StaticAutoPtr<FlaggedArray<T>> ListenerCollection<T>::gListeners;
template <class T>
StaticAutoPtr<FlaggedArray<T>> ListenerCollection<T>::gListenersToRemove;

using JSListeners = ListenerCollection<RefPtr<PlacesEventCallback>>;
using WeakJSListeners = ListenerCollection<WeakPtr<PlacesWeakCallbackWrapper>>;
using WeakNativeListeners =
    ListenerCollection<WeakPtr<places::INativePlacesEventCallback>>;

// Even if NotifyListeners is called any timing, we mange the notifications with
// adding to this queue, then sending in sequence. This avoids sending nested
// notifications while previous ones are still being sent.
static nsTArray<Sequence<OwningNonNull<PlacesEvent>>> gNotificationQueue;

uint32_t GetEventTypeFlag(PlacesEventType aEventType) {
  if (aEventType == PlacesEventType::None) {
    return 0;
  }
  return 1 << ((uint32_t)aEventType - 1);
}

uint32_t GetFlagsForEventTypes(const nsTArray<PlacesEventType>& aEventTypes) {
  uint32_t flags = 0;
  for (PlacesEventType eventType : aEventTypes) {
    flags |= GetEventTypeFlag(eventType);
  }
  return flags;
}

uint32_t GetFlagsForEvents(
    const nsTArray<OwningNonNull<PlacesEvent>>& aEvents) {
  uint32_t flags = 0;
  for (const PlacesEvent& event : aEvents) {
    flags |= GetEventTypeFlag(event.Type());
  }
  return flags;
}

template <class TWrapped, class TUnwrapped, class TListenerCollection>
MOZ_CAN_RUN_SCRIPT void CallListeners(
    uint32_t aEventFlags, const Sequence<OwningNonNull<PlacesEvent>>& aEvents,
    unsigned long aListenersLengthToCall,
    const std::function<TUnwrapped(TWrapped&)>& aUnwrapListener,
    const std::function<void(TUnwrapped&,
                             const Sequence<OwningNonNull<PlacesEvent>>&)>&
        aCallListener) {
  auto& listeners = *TListenerCollection::GetListeners();
  for (uint32_t i = 0; i < aListenersLengthToCall; i++) {
    Flagged<TWrapped>& listener = listeners[i];
    TUnwrapped unwrapped = aUnwrapListener(listener.value);
    if (!unwrapped) {
      continue;
    }

    if ((listener.flags & aEventFlags) == aEventFlags) {
      aCallListener(unwrapped, aEvents);
    } else if (listener.flags & aEventFlags) {
      Sequence<OwningNonNull<PlacesEvent>> filtered;
      for (const OwningNonNull<PlacesEvent>& event : aEvents) {
        if (listener.flags & GetEventTypeFlag(event->Type())) {
          bool success = !!filtered.AppendElement(event, fallible);
          MOZ_RELEASE_ASSERT(success);
        }
      }
      aCallListener(unwrapped, filtered);
    }
  }
}

void PlacesObservers::AddListener(GlobalObject& aGlobal,
                                  const nsTArray<PlacesEventType>& aEventTypes,
                                  PlacesEventCallback& aCallback,
                                  ErrorResult& rv) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<RefPtr<PlacesEventCallback>>* listeners =
      JSListeners::GetListeners();
  Flagged<RefPtr<PlacesEventCallback>> pair(flags, &aCallback);
  listeners->AppendElement(pair);
}

void PlacesObservers::AddListener(GlobalObject& aGlobal,
                                  const nsTArray<PlacesEventType>& aEventTypes,
                                  PlacesWeakCallbackWrapper& aCallback,
                                  ErrorResult& rv) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>* listeners =
      WeakJSListeners::GetListeners();
  WeakPtr<PlacesWeakCallbackWrapper> weakCb(&aCallback);
  MOZ_ASSERT(weakCb.get());
  Flagged<WeakPtr<PlacesWeakCallbackWrapper>> flagged(flags, std::move(weakCb));
  listeners->AppendElement(flagged);
}

void PlacesObservers::AddListener(
    const nsTArray<PlacesEventType>& aEventTypes,
    places::INativePlacesEventCallback* aCallback) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>* listeners =
      WeakNativeListeners::GetListeners();
  Flagged<WeakPtr<places::INativePlacesEventCallback>> pair(flags, aCallback);
  listeners->AppendElement(pair);
}

void PlacesObservers::RemoveListener(
    GlobalObject& aGlobal, const nsTArray<PlacesEventType>& aEventTypes,
    PlacesEventCallback& aCallback, ErrorResult& rv) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (!gNotificationQueue.IsEmpty()) {
    FlaggedArray<RefPtr<PlacesEventCallback>>* listeners =
        JSListeners::GetListenersToRemove();
    Flagged<RefPtr<PlacesEventCallback>> pair(flags, &aCallback);
    listeners->AppendElement(pair);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void PlacesObservers::RemoveListener(
    GlobalObject& aGlobal, const nsTArray<PlacesEventType>& aEventTypes,
    PlacesWeakCallbackWrapper& aCallback, ErrorResult& rv) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (!gNotificationQueue.IsEmpty()) {
    FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>* listeners =
        WeakJSListeners::GetListenersToRemove();
    WeakPtr<PlacesWeakCallbackWrapper> weakCb(&aCallback);
    MOZ_ASSERT(weakCb.get());
    Flagged<WeakPtr<PlacesWeakCallbackWrapper>> flagged(flags,
                                                        std::move(weakCb));
    listeners->AppendElement(flagged);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void PlacesObservers::RemoveListener(
    const nsTArray<PlacesEventType>& aEventTypes,
    places::INativePlacesEventCallback* aCallback) {
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (!gNotificationQueue.IsEmpty()) {
    FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>* listeners =
        WeakNativeListeners::GetListenersToRemove();
    Flagged<WeakPtr<places::INativePlacesEventCallback>> pair(flags, aCallback);
    listeners->AppendElement(pair);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void PlacesObservers::RemoveListener(uint32_t aFlags,
                                     PlacesEventCallback& aCallback) {
  FlaggedArray<RefPtr<PlacesEventCallback>>* listeners =
      JSListeners::GetListeners(/* aDoNotInit: */ true);
  if (!listeners) {
    return;
  }
  for (uint32_t i = 0; i < listeners->Length(); i++) {
    Flagged<RefPtr<PlacesEventCallback>>& l = listeners->ElementAt(i);
    if (!(*l.value == aCallback)) {
      continue;
    }
    if (l.flags == (aFlags & l.flags)) {
      listeners->RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

void PlacesObservers::RemoveListener(uint32_t aFlags,
                                     PlacesWeakCallbackWrapper& aCallback) {
  FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>* listeners =
      WeakJSListeners::GetListeners(/* aDoNotInit: */ true);
  if (!listeners) {
    return;
  }
  for (uint32_t i = 0; i < listeners->Length(); i++) {
    Flagged<WeakPtr<PlacesWeakCallbackWrapper>>& l = listeners->ElementAt(i);
    RefPtr<PlacesWeakCallbackWrapper> unwrapped = l.value.get();
    if (unwrapped != &aCallback) {
      continue;
    }
    if (l.flags == (aFlags & l.flags)) {
      listeners->RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

void PlacesObservers::RemoveListener(
    uint32_t aFlags, places::INativePlacesEventCallback* aCallback) {
  FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>* listeners =
      WeakNativeListeners::GetListeners(/* aDoNotInit: */ true);
  if (!listeners) {
    return;
  }
  for (uint32_t i = 0; i < listeners->Length(); i++) {
    Flagged<WeakPtr<places::INativePlacesEventCallback>>& l =
        listeners->ElementAt(i);
    RefPtr<places::INativePlacesEventCallback> unwrapped = l.value.get();
    if (unwrapped != aCallback) {
      continue;
    }
    if (l.flags == (aFlags & l.flags)) {
      listeners->RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

template <class TWrapped, class TUnwrapped, class TListenerCollection>
void CleanupListeners(
    const std::function<TUnwrapped(TWrapped&)>& aUnwrapListener,
    const std::function<void(Flagged<TWrapped>&)>& aRemoveListener) {
  auto& listeners = *TListenerCollection::GetListeners();
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    Flagged<TWrapped>& listener = listeners[i];
    TUnwrapped unwrapped = aUnwrapListener(listener.value);
    if (!unwrapped) {
      listeners.RemoveElementAt(i);
      i--;
    }
  }

  auto& listenersToRemove = *TListenerCollection::GetListenersToRemove();
  for (auto& listener : listenersToRemove) {
    aRemoveListener(listener);
  }
  listenersToRemove.Clear();
}

void PlacesObservers::NotifyListeners(
    GlobalObject& aGlobal, const Sequence<OwningNonNull<PlacesEvent>>& aEvents,
    ErrorResult& rv) {
  NotifyListeners(aEvents);
}

void PlacesObservers::NotifyListeners(
    const Sequence<OwningNonNull<PlacesEvent>>& aEvents) {
  MOZ_ASSERT(aEvents.Length() > 0, "Must pass a populated array of events");
  if (aEvents.Length() == 0) {
    return;
  }

#ifdef DEBUG
  if (!gNotificationQueue.IsEmpty()) {
    NS_WARNING(
        "Avoid nested Places notifications if possible, the order of events "
        "cannot be guaranteed");
    nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();
    Unused << xpc->DebugDumpJSStack(false, false, false);
  }
#endif

  gNotificationQueue.AppendElement(aEvents);

  // If gNotificationQueue has only the events we added now, start to notify.
  // Otherwise, as it already started the notification processing,
  // rely on the processing.
  if (gNotificationQueue.Length() == 1) {
    NotifyNext();
  }
}

void PlacesObservers::NotifyNext() {
  auto events = gNotificationQueue[0];
  uint32_t flags = GetFlagsForEvents(events);

  // Send up to the number of current listeners, to avoid handling listeners
  // added during this notification.
  unsigned long jsListenersLength = JSListeners::GetListeners()->Length();
  unsigned long weakNativeListenersLength =
      WeakNativeListeners::GetListeners()->Length();
  unsigned long weakJSListenersLength =
      WeakJSListeners::GetListeners()->Length();

  CallListeners<RefPtr<PlacesEventCallback>, RefPtr<PlacesEventCallback>,
                JSListeners>(
      flags, events, jsListenersLength, [](auto& cb) { return cb; },
      // MOZ_CAN_RUN_SCRIPT_BOUNDARY because on Windows this gets called from
      // some internals of the std::function implementation that we can't
      // annotate.  We handle this by annotating CallListeners and making sure
      // it holds a strong ref to the callback.
      [&](auto& cb, const auto& events)
          MOZ_CAN_RUN_SCRIPT_BOUNDARY { MOZ_KnownLive(cb)->Call(events); });

  CallListeners<WeakPtr<places::INativePlacesEventCallback>,
                RefPtr<places::INativePlacesEventCallback>,
                WeakNativeListeners>(
      flags, events, weakNativeListenersLength,
      [](auto& cb) { return cb.get(); },
      [&](auto& cb, const Sequence<OwningNonNull<PlacesEvent>>& events) {
        cb->HandlePlacesEvent(events);
      });

  CallListeners<WeakPtr<PlacesWeakCallbackWrapper>,
                RefPtr<PlacesWeakCallbackWrapper>, WeakJSListeners>(
      flags, events, weakJSListenersLength, [](auto& cb) { return cb.get(); },
      // MOZ_CAN_RUN_SCRIPT_BOUNDARY because on Windows this gets called from
      // some internals of the std::function implementation that we can't
      // annotate.  We handle this by annotating CallListeners and making sure
      // it holds a strong ref to the callback.
      [&](auto& cb, const auto& events) MOZ_CAN_RUN_SCRIPT_BOUNDARY {
        RefPtr<PlacesEventCallback> callback(cb->mCallback);
        callback->Call(events);
      });

  gNotificationQueue.RemoveElementAt(0);

  CleanupListeners<RefPtr<PlacesEventCallback>, RefPtr<PlacesEventCallback>,
                   JSListeners>(
      [](auto& cb) { return cb; },
      [&](auto& cb) { RemoveListener(cb.flags, *cb.value); });
  CleanupListeners<WeakPtr<PlacesWeakCallbackWrapper>,
                   RefPtr<PlacesWeakCallbackWrapper>, WeakJSListeners>(
      [](auto& cb) { return cb.get(); },
      [&](auto& cb) { RemoveListener(cb.flags, *cb.value.get()); });
  CleanupListeners<WeakPtr<places::INativePlacesEventCallback>,
                   RefPtr<places::INativePlacesEventCallback>,
                   WeakNativeListeners>(
      [](auto& cb) { return cb.get(); },
      [&](auto& cb) { RemoveListener(cb.flags, cb.value.get()); });

  if (!gNotificationQueue.IsEmpty()) {
    NotifyNext();
  }
}

}  // namespace mozilla::dom
