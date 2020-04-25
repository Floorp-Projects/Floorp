/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EventSourceEventService_h
#define mozilla_dom_EventSourceEventService_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Atomics.h"
#include "nsIEventSourceEventService.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class EventSourceEventService final : public nsIEventSourceEventService,
                                      public nsIObserver {
  friend class EventSourceBaseRunnable;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIEVENTSOURCEEVENTSERVICE

  static already_AddRefed<EventSourceEventService> GetOrCreate();

  void EventSourceConnectionOpened(uint64_t aHttpChannelId,
                                   uint64_t aInnerWindowID);

  void EventSourceConnectionClosed(uint64_t aHttpChannelId,
                                   uint64_t aInnerWindowID);

  void EventReceived(uint64_t aHttpChannelId, uint64_t aInnerWindowID,
                     const nsAString& aEventName, const nsAString& aLastEventID,
                     const nsAString& aData, uint32_t aRetry,
                     DOMHighResTimeStamp aTimeStamp);

 private:
  EventSourceEventService();
  ~EventSourceEventService();

  bool HasListeners() const;
  void Shutdown();

  using EventSourceListeners = nsTArray<nsCOMPtr<nsIEventSourceEventListener>>;

  struct WindowListener {
    EventSourceListeners mListeners;
  };

  void GetListeners(uint64_t aInnerWindowID,
                    EventSourceListeners& aListeners) const;

  // Used only on the main-thread.
  nsClassHashtable<nsUint64HashKey, WindowListener> mWindows;

  Atomic<uint64_t> mCountListeners;
};

}  // namespace dom
}  // namespace mozilla

/**
 * Casting EventSourceEventService to nsISupports is ambiguous.
 * This method handles that.
 */
inline nsISupports* ToSupports(mozilla::dom::EventSourceEventService* p) {
  return NS_ISUPPORTS_CAST(nsIEventSourceEventService*, p);
}

#endif  // mozilla_dom_EventSourceEventService_h
