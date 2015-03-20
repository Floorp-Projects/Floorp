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

class CacheStreamControlChild MOZ_FINAL : public PCacheStreamControlChild
                                        , public StreamControl
                                        , public ActorChild
{
public:
  CacheStreamControlChild();
  ~CacheStreamControlChild();

  // ActorChild methods
  virtual void StartDestroy() MOZ_OVERRIDE;

  // StreamControl methods
  virtual void
  SerializeControl(PCacheReadStream* aReadStreamOut) MOZ_OVERRIDE;

  virtual void
  SerializeFds(PCacheReadStream* aReadStreamOut,
               const nsTArray<mozilla::ipc::FileDescriptor>& aFds) MOZ_OVERRIDE;

  virtual void
  DeserializeFds(const PCacheReadStream& aReadStream,
                 nsTArray<mozilla::ipc::FileDescriptor>& aFdsOut) MOZ_OVERRIDE;

private:
  virtual void
  NoteClosedAfterForget(const nsID& aId) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void
  AssertOwningThread() MOZ_OVERRIDE;
#endif

  // PCacheStreamControlChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvClose(const nsID& aId) MOZ_OVERRIDE;
  virtual bool RecvCloseAll() MOZ_OVERRIDE;

  bool mDestroyStarted;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStreamControlChild_h
