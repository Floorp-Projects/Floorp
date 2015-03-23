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
public:
  CachePushStreamChild(Feature* aFeature, nsIAsyncInputStream* aStream);
  ~CachePushStreamChild();

  virtual void StartDestroy() override;

  void Start();

private:
  class Callback;

  // PCachePushStreamChild methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  void DoRead();

  void Wait();

  void OnStreamReady(Callback* aCallback);

  void OnEnd(nsresult aRv);

  nsCOMPtr<nsIAsyncInputStream> mStream;
  nsRefPtr<Callback> mCallback;
  bool mClosed;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CachePushStreamChild_h
