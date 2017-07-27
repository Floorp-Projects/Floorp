/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageNotifierService_h
#define mozilla_dom_StorageNotifierService_h

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class StorageEvent;

class StorageNotificationObserver
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void
  ObserveStorageNotification(StorageEvent* aEvent,
                             const char16_t* aStorageType,
                             bool aPrivateBrowsing) = 0;

  virtual bool
  IsPrivateBrowsing() const = 0;

  virtual nsIPrincipal*
  GetPrincipal() const = 0;

  virtual nsIEventTarget*
  GetEventTarget() const = 0;
};

class StorageNotifierService final
{
public:
  NS_INLINE_DECL_REFCOUNTING(StorageNotifierService)

  static StorageNotifierService*
  GetOrCreate();

  static void
  Broadcast(StorageEvent* aEvent, const char16_t* aStorageType,
            bool aPrivateBrowsing, bool aImmediateDispatch);

  void
  Register(StorageNotificationObserver* aObserver);

  void
  Unregister(StorageNotificationObserver* aObserver);

private:
  StorageNotifierService();
  ~StorageNotifierService();

  nsTObserverArray<RefPtr<StorageNotificationObserver>> mObservers;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageNotifierService_h
