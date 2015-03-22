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
  SerializeControl(PCacheReadStream* aReadStreamOut) override;

  virtual void
  SerializeFds(PCacheReadStream* aReadStreamOut,
               const nsTArray<mozilla::ipc::FileDescriptor>& aFds) override;

  virtual void
  DeserializeFds(const PCacheReadStream& aReadStream,
                 nsTArray<mozilla::ipc::FileDescriptor>& aFdsOut) override;

private:
  virtual void
  NoteClosedAfterForget(const nsID& aId) override;

#ifdef DEBUG
  virtual void
  AssertOwningThread() override;
#endif

  // PCacheStreamControlChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;
  virtual bool RecvClose(const nsID& aId) override;
  virtual bool RecvCloseAll() override;

  bool mDestroyStarted;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStreamControlChild_h
