/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStreamControlParent_h
#define mozilla_dom_cache_CacheStreamControlParent_h

#include "mozilla/dom/cache/PCacheStreamControlParent.h"
#include "mozilla/dom/cache/StreamControl.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace ipc {
class AutoIPCStream;
} // namespace ipc
namespace dom {
namespace cache {

class ReadStream;
class StreamList;

class CacheStreamControlParent final : public PCacheStreamControlParent
                                     , public StreamControl
{
public:
  CacheStreamControlParent();
  ~CacheStreamControlParent();

  void SetStreamList(StreamList* aStreamList);
  void Close(const nsID& aId);
  void CloseAll();
  void Shutdown();

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

  // PCacheStreamControlParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;
  virtual mozilla::ipc::IPCResult RecvNoteClosed(const nsID& aId) override;

  void NotifyClose(const nsID& aId);
  void NotifyCloseAll();

  // Cycle with StreamList via a weak-ref to us.  Cleanup occurs when the actor
  // is deleted by the PBackground manager.  ActorDestroy() then calls
  // StreamList::RemoveStreamControl() to clear the weak ref.
  RefPtr<StreamList> mStreamList;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStreamControlParent_h
