/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStreamControlChild_h
#define mozilla_dom_cache_CacheStreamControlChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/PCacheStreamControlChild.h"
#include "mozilla/dom/cache/StreamControl.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace ipc {
class AutoIPCStream;
} // namespace ipc
namespace dom {
namespace cache {

class ReadStream;

class CacheStreamControlChild final : public PCacheStreamControlChild
                                    , public StreamControl
                                    , public ActorChild
{
public:
  CacheStreamControlChild();
  ~CacheStreamControlChild();

  // ActorChild methods
  virtual void StartDestroy() override;

  // StreamControl methods
  virtual void
  SerializeControl(CacheReadStream* aReadStreamOut) override;

  virtual void
  SerializeStream(CacheReadStream* aReadStreamOut, nsIInputStream* aStream,
                  nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>>& aStreamCleanupList) override;

private:
  virtual void
  NoteClosedAfterForget(const nsID& aId) override;

#ifdef DEBUG
  virtual void
  AssertOwningThread() override;
#endif

  // PCacheStreamControlChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;
  virtual mozilla::ipc::IPCResult RecvClose(const nsID& aId) override;
  virtual mozilla::ipc::IPCResult RecvCloseAll() override;

  bool mDestroyStarted;
  bool mDestroyDelayed;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStreamControlChild_h
