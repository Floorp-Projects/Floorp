/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CachePushStreamChild_h
#define mozilla_dom_cache_CachePushStreamChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/PCachePushStreamChild.h"
#include "nsCOMPtr.h"

class nsIAsyncInputStream;

namespace mozilla {
namespace dom {
namespace cache {

class CachePushStreamChild final : public PCachePushStreamChild
                                 , public ActorChild
{
  friend class CacheChild;

public:
  void Start();

  virtual void StartDestroy() override;

private:
  class Callback;

  // This class must be constructed using CacheChild::CreatePushStream()
  CachePushStreamChild(Feature* aFeature, nsISupports* aParent,
                       nsIAsyncInputStream* aStream);
  ~CachePushStreamChild();

  // PCachePushStreamChild methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  void DoRead();

  void Wait();

  void OnStreamReady(Callback* aCallback);

  void OnEnd(nsresult aRv);

  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<Callback> mCallback;
  bool mClosed;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CachePushStreamChild_h
