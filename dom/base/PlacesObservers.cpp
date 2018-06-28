/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlacesObservers.h"

#include "PlacesWeakCallbackWrapper.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace dom {

template <class T>
struct Flagged
{
  Flagged(uint32_t aFlags, T&& aValue)
    : flags(aFlags)
    , value(std::forward<T>(aValue))
  {}
  Flagged(Flagged&& aOther)
    : Flagged(std::move(aOther.flags), std::move(aOther.value))
  {}
  Flagged(const Flagged& aOther) = default;
  ~Flagged() = default;

  uint32_t flags;
  T value;
};

template <class T>
using FlaggedArray = nsTArray<Flagged<T>>;

template <class T>
struct ListenerCollection
{
  static StaticAutoPtr<FlaggedArray<T>> gListeners;
  static StaticAutoPtr<FlaggedArray<T>> gListenersToRemove;

  static FlaggedArray<T>* GetListeners() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gListeners) {
      gListeners = new FlaggedArray<T>();
      ClearOnShutdown(&gListeners);
    }
    return gListeners;
  }

  static FlaggedArray<T>* GetListenersToRemove() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gListenersToRemove) {
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

typedef ListenerCollection<RefPtr<PlacesEventCallback>> JSListeners;
typedef ListenerCollection<WeakPtr<PlacesWeakCallbackWrapper>> WeakJSListeners;
typedef ListenerCollection<WeakPtr<places::INativePlacesEventCallback>> WeakNativeListeners;

static bool gCallingListeners = false;

uint32_t
GetEventTypeFlag(PlacesEventType aEventType)
{
  if (aEventType == PlacesEventType::None) {
    return 0;
  }
  return 1 << ((uint32_t)aEventType - 1);
}

uint32_t
GetFlagsForEventTypes(const nsTArray<PlacesEventType>& aEventTypes)
{
  uint32_t flags = 0;
  for (PlacesEventType eventType : aEventTypes) {
    flags |= GetEventTypeFlag(eventType);
  }
  return flags;
}

uint32_t
GetFlagsForEvents(const nsTArray<OwningNonNull<PlacesEvent>>& aEvents)
{
  uint32_t flags = 0;
  for (const PlacesEvent& event : aEvents) {
    flags |= GetEventTypeFlag(event.Type());
  }
  return flags;
}

template <class TWrapped, class TUnwrapped>
void CallListeners(uint32_t aEventFlags,
                   FlaggedArray<TWrapped>& aListeners,
                   const Sequence<OwningNonNull<PlacesEvent>>& aEvents,
                   const std::function<TUnwrapped(TWrapped&)>& aUnwrapListener,
                   const std::function<void(TUnwrapped&, const Sequence<OwningNonNull<PlacesEvent>>&)>& aCallListener)
{
  for (uint32_t i = 0; i < aListeners.Length(); i++) {
    Flagged<TWrapped>& l = aListeners[i];
    TUnwrapped unwrapped = aUnwrapListener(l.value);
    if (!unwrapped) {
      aListeners.RemoveElementAt(i);
      i--;
      continue;
    }

    if ((l.flags & aEventFlags) == aEventFlags) {
      aCallListener(unwrapped, aEvents);
    } else if (l.flags & aEventFlags) {
      Sequence<OwningNonNull<PlacesEvent>> filtered;
      for (const OwningNonNull<PlacesEvent>& event : aEvents) {
        if (l.flags & GetEventTypeFlag(event->Type())) {
          bool success = !!filtered.AppendElement(event, fallible);
          MOZ_RELEASE_ASSERT(success);
        }
      }
      aCallListener(unwrapped, filtered);
    }
  }
}

void
PlacesObservers::AddListener(GlobalObject& aGlobal,
                             const nsTArray<PlacesEventType>& aEventTypes,
                             PlacesEventCallback& aCallback,
                             ErrorResult& rv)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<RefPtr<PlacesEventCallback>>* listeners =
    JSListeners::GetListeners();
  Flagged<RefPtr<PlacesEventCallback>> pair(flags, &aCallback);
  listeners->AppendElement(pair);
}

void
PlacesObservers::AddListener(GlobalObject& aGlobal,
                             const nsTArray<PlacesEventType>& aEventTypes,
                             PlacesWeakCallbackWrapper& aCallback,
                             ErrorResult& rv)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>* listeners =
    WeakJSListeners::GetListeners();
  WeakPtr<PlacesWeakCallbackWrapper> weakCb(&aCallback);
  MOZ_ASSERT(weakCb.get());
  Flagged<WeakPtr<PlacesWeakCallbackWrapper>> flagged(flags, std::move(weakCb));
  listeners->AppendElement(flagged);
}

void
PlacesObservers::AddListener(const nsTArray<PlacesEventType>& aEventTypes,
                             places::INativePlacesEventCallback* aCallback)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);

  FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>* listeners =
    WeakNativeListeners::GetListeners();
  Flagged<WeakPtr<places::INativePlacesEventCallback>> pair(flags, aCallback);
  listeners->AppendElement(pair);
}

void
PlacesObservers::RemoveListener(GlobalObject& aGlobal,
                                const nsTArray<PlacesEventType>& aEventTypes,
                                PlacesEventCallback& aCallback,
                                ErrorResult& rv)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (gCallingListeners) {
    FlaggedArray<RefPtr<PlacesEventCallback>>* listeners =
      JSListeners::GetListenersToRemove();
    Flagged<RefPtr<PlacesEventCallback>> pair(flags, &aCallback);
    listeners->AppendElement(pair);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void
PlacesObservers::RemoveListener(GlobalObject& aGlobal,
                                const nsTArray<PlacesEventType>& aEventTypes,
                                PlacesWeakCallbackWrapper& aCallback,
                                ErrorResult& rv)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (gCallingListeners) {
    FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>* listeners =
      WeakJSListeners::GetListenersToRemove();
    WeakPtr<PlacesWeakCallbackWrapper> weakCb(&aCallback);
    MOZ_ASSERT(weakCb.get());
    Flagged<WeakPtr<PlacesWeakCallbackWrapper>> flagged(flags, std::move(weakCb));
    listeners->AppendElement(flagged);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void
PlacesObservers::RemoveListener(const nsTArray<PlacesEventType>& aEventTypes,
                                places::INativePlacesEventCallback* aCallback)
{
  uint32_t flags = GetFlagsForEventTypes(aEventTypes);
  if (gCallingListeners) {
    FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>* listeners =
      WeakNativeListeners::GetListenersToRemove();
    Flagged<WeakPtr<places::INativePlacesEventCallback>> pair(flags, aCallback);
    listeners->AppendElement(pair);
  } else {
    RemoveListener(flags, aCallback);
  }
}

void
PlacesObservers::RemoveListener(uint32_t aFlags,
                                PlacesEventCallback& aCallback)
{
  FlaggedArray<RefPtr<PlacesEventCallback>>& listeners =
    *JSListeners::GetListeners();
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    Flagged<RefPtr<PlacesEventCallback>>& l = listeners[i];
    if (!(*l.value == aCallback)) {
      continue;
    }
    if (l.flags == aFlags) {
      listeners.RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

void
PlacesObservers::RemoveListener(uint32_t aFlags,
                                PlacesWeakCallbackWrapper& aCallback)
{
  FlaggedArray<WeakPtr<PlacesWeakCallbackWrapper>>& listeners =
    *WeakJSListeners::GetListeners();
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    Flagged<WeakPtr<PlacesWeakCallbackWrapper>>& l = listeners[i];
    RefPtr<PlacesWeakCallbackWrapper> unwrapped = l.value.get();
    if (unwrapped != &aCallback) {
      continue;
    }
    if (l.flags == aFlags) {
      listeners.RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

void
PlacesObservers::RemoveListener(uint32_t aFlags,
                                places::INativePlacesEventCallback* aCallback)
{
  FlaggedArray<WeakPtr<places::INativePlacesEventCallback>>& listeners =
    *WeakNativeListeners::GetListeners();
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    Flagged<WeakPtr<places::INativePlacesEventCallback>>& l = listeners[i];
    RefPtr<places::INativePlacesEventCallback> unwrapped = l.value.get();
    if (unwrapped != aCallback) {
      continue;
    }
    if (l.flags == aFlags) {
      listeners.RemoveElementAt(i);
      i--;
    } else {
      l.flags &= ~aFlags;
    }
  }
}

void
PlacesObservers::NotifyListeners(GlobalObject& aGlobal,
                                 const Sequence<OwningNonNull<PlacesEvent>>& aEvents,
                                 ErrorResult& rv)
{
  NotifyListeners(aEvents);
}

void
PlacesObservers::NotifyListeners(const Sequence<OwningNonNull<PlacesEvent>>& aEvents)
{
  MOZ_RELEASE_ASSERT(!gCallingListeners);
  gCallingListeners = true;
  uint32_t flags = GetFlagsForEvents(aEvents);

  CallListeners<RefPtr<PlacesEventCallback>, RefPtr<PlacesEventCallback>>(
    flags, *JSListeners::GetListeners(), aEvents,
    [](auto& cb) { return cb; },
    [&](auto& cb, const auto& events) {
      cb->Call(aEvents);
    });

  CallListeners<WeakPtr<places::INativePlacesEventCallback>,
                RefPtr<places::INativePlacesEventCallback>>(
    flags, *WeakNativeListeners::GetListeners(), aEvents,
    [](auto& cb) { return cb.get(); },
    [&](auto& cb, const Sequence<OwningNonNull<PlacesEvent>>& events) {
      cb->HandlePlacesEvent(events);
    });

  CallListeners<WeakPtr<PlacesWeakCallbackWrapper>,
                RefPtr<PlacesWeakCallbackWrapper>>(
    flags, *WeakJSListeners::GetListeners(), aEvents,
    [](auto& cb) { return cb.get(); },
    [&](auto& cb, const auto& events) {
      cb->mCallback->Call(aEvents);
    });

  auto& listenersToRemove = *JSListeners::GetListenersToRemove();
  if (listenersToRemove.Length() > 0) {
    for (auto& listener : listenersToRemove) {
      RemoveListener(listener.flags, *listener.value);
    }
  }

  auto& weakListenersToRemove = *WeakJSListeners::GetListenersToRemove();
  if (weakListenersToRemove.Length() > 0) {
    for (auto& listener : weakListenersToRemove) {
      RemoveListener(listener.flags, *listener.value.get());
    }
  }

  auto& nativeListenersToRemove = *WeakNativeListeners::GetListenersToRemove();
  if (nativeListenersToRemove.Length() > 0) {
    for (auto& listener : nativeListenersToRemove) {
      RemoveListener(listener.flags, listener.value.get());
    }
  }

  gCallingListeners = false;
}

} // namespace dom
} // namespace mozilla
