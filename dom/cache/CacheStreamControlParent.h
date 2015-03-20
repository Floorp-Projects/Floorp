/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStreamControlParent_h
#define mozilla_dom_cache_CacheStreamControlParent_h

#include "mozilla/dom/cache/PCacheStreamControlParent.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace dom {
namespace cache {

class ReadStream;
class StreamList;

class CacheStreamControlParent : public PCacheStreamControlParent
{
public:
  CacheStreamControlParent();
  ~CacheStreamControlParent();

  void AddListener(ReadStream* aListener);
  void RemoveListener(ReadStream* aListener);

  void SetStreamList(StreamList* aStreamList);
  void Close(const nsID& aId);
  void CloseAll();
  void Shutdown();

  // PCacheStreamControlParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvNoteClosed(const nsID& aId) MOZ_OVERRIDE;

private:
  void NotifyClose(const nsID& aId);
  void NotifyCloseAll();

  // Cycle with StreamList via a weak-ref to us.  Cleanup occurs when the actor
  // is deleted by the PBackground manager.  ActorDestroy() then calls
  // StreamList::RemoveStreamControl() to clear the weak ref.
  nsRefPtr<StreamList> mStreamList;

  typedef nsTObserverArray<ReadStream*> ReadStreamList;
  ReadStreamList mListeners;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStreamControlParent_h
