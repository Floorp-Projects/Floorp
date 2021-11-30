/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaUtils.h"
#include "mozilla/Services.h"

namespace mozilla::media {

nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
  if (!svc) {
    // We can fail to get the shutdown service if we're already shutting down.
    return nullptr;
  }

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));
  if (!barrier) {
    // We are probably in a content process. We need to do cleanup at
    // XPCOM shutdown in leakchecking builds.
    rv = svc->GetXpcomWillShutdown(getter_AddRefs(barrier));
  }
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(barrier);
  return barrier;
}

nsCOMPtr<nsIAsyncShutdownClient> MustGetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  MOZ_RELEASE_ASSERT(barrier);
  return barrier;
}

NS_IMPL_ISUPPORTS(ShutdownBlocker, nsIAsyncShutdownBlocker)

namespace {
class MediaEventBlocker : public ShutdownBlocker {
 public:
  explicit MediaEventBlocker(nsString aName)
      : ShutdownBlocker(std::move(aName)) {}

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aProfileBeforeChange) override {
    mShutdownEvent.Notify();
    return NS_OK;
  }

  MediaEventSource<void>& ShutdownEvent() { return mShutdownEvent; }

 private:
  MediaEventProducer<void> mShutdownEvent;
};

class RefCountedTicket {
  RefPtr<MediaEventBlocker> mBlocker;
  MediaEventForwarder<void> mShutdownEventForwarder;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedTicket)

  RefCountedTicket()
      : mShutdownEventForwarder(GetMainThreadSerialEventTarget()) {}

  void AddBlocker(const nsString& aName, const nsString& aFileName,
                  int32_t aLineNr) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mBlocker);
    mBlocker = MakeAndAddRef<MediaEventBlocker>(aName);
    mShutdownEventForwarder.Forward(mBlocker->ShutdownEvent());
    GetShutdownBarrier()->AddBlocker(mBlocker.get(), aFileName, aLineNr, aName);
  }

  MediaEventSource<void>& ShutdownEvent() { return mShutdownEventForwarder; }

 protected:
  virtual ~RefCountedTicket() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlocker);
    GetShutdownBarrier()->RemoveBlocker(mBlocker.get());
    mShutdownEventForwarder.DisconnectAll();
  }
};

class ShutdownBlockingTicketImpl : public ShutdownBlockingTicket {
 private:
  RefPtr<RefCountedTicket> mTicket;

 public:
  ShutdownBlockingTicketImpl(nsString aName, nsString aFileName,
                             int32_t aLineNr)
      : mTicket(MakeAndAddRef<RefCountedTicket>()) {
    aName.AppendPrintf(" - %p", this);
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__, [ticket = mTicket, name = std::move(aName),
                   fileName = std::move(aFileName), lineNr = aLineNr] {
          ticket->AddBlocker(name, fileName, lineNr);
        }));
  }

  ~ShutdownBlockingTicketImpl() {
    NS_ReleaseOnMainThread(__func__, mTicket.forget(), true);
  }

  MediaEventSource<void>& ShutdownEvent() override {
    return mTicket->ShutdownEvent();
  }
};
}  // namespace

UniquePtr<ShutdownBlockingTicket> ShutdownBlockingTicket::Create(
    nsString aName, nsString aFileName, int32_t aLineNr) {
  return WrapUnique(new ShutdownBlockingTicketImpl(
      std::move(aName), std::move(aFileName), aLineNr));
}

}  // namespace mozilla::media
