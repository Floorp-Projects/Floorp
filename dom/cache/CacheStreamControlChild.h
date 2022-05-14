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

namespace mozilla::dom::cache {

class ReadStream;

class CacheStreamControlChild final : public PCacheStreamControlChild,
                                      public StreamControl,
                                      public ActorChild {
  friend class PCacheStreamControlChild;

 public:
  CacheStreamControlChild();

  // ActorChild methods
  virtual void StartDestroy() override;

  // StreamControl methods
  virtual void SerializeControl(CacheReadStream* aReadStreamOut) override;

  virtual void SerializeStream(CacheReadStream* aReadStreamOut,
                               nsIInputStream* aStream) override;

  virtual void OpenStream(const nsID& aId,
                          InputStreamResolver&& aResolver) override;

  NS_DECL_OWNINGTHREAD
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheStreamControlChild, override)
 private:
  ~CacheStreamControlChild();
  virtual void NoteClosedAfterForget(const nsID& aId) override;

#ifdef DEBUG
  virtual void AssertOwningThread() override;
#endif

  // PCacheStreamControlChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;
  mozilla::ipc::IPCResult RecvClose(const nsID& aId);
  mozilla::ipc::IPCResult RecvCloseAll();

  bool mDestroyStarted;
  bool mDestroyDelayed;
};

}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_CacheStreamControlChild_h
