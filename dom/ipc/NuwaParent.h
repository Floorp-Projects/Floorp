/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NuwaParent_h
#define mozilla_dom_NuwaParent_h

#include "base/message_loop.h"
#include "mozilla/dom/PNuwaParent.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

class ContentParent;

class NuwaParent : public mozilla::dom::PNuwaParent
{
public:
  explicit NuwaParent();

  // Called on the main thread.
  bool ForkNewProcess(uint32_t& aPid,
                      UniquePtr<nsTArray<ProtocolFdMapping>>&& aFds,
                      bool aBlocking);

  // Called on the background thread.
  bool ActorConstructed();

  // Both the worker thread and the main thread hold a ref to this.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NuwaParent)

  // Functions to be invoked by the manager of this actor to alloc/dealloc the
  // actor.
  static NuwaParent* Alloc();
  static bool ActorConstructed(mozilla::dom::PNuwaParent *aActor);
  static bool Dealloc(mozilla::dom::PNuwaParent *aActor);

protected:
  virtual ~NuwaParent();

  virtual bool RecvNotifyReady() override;
  virtual bool RecvAddNewProcess(const uint32_t& aPid,
                                 nsTArray<ProtocolFdMapping>&& aFds) override;
  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                ProtocolCloneContext* aCtx) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  void AssertIsOnWorkerThread();

  bool mBlocked;
  mozilla::Monitor mMonitor;
  NuwaParent* mClonedActor;

  nsCOMPtr<nsIThread> mWorkerThread;

  uint32_t mNewProcessPid;
  UniquePtr<nsTArray<ProtocolFdMapping>> mNewProcessFds;

  // The mutual reference will be broken on the main thread.
  RefPtr<ContentParent> mContentParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_NuwaParent_h
