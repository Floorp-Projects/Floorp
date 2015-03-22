/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CachePushStreamParent_h
#define mozilla_dom_cache_CachePushStreamParent_h

#include "mozilla/dom/cache/PCachePushStreamParent.h"

class nsIAsyncInputStream;
class nsIAsyncOutputStream;
class nsIInputStream;

namespace mozilla {
namespace dom {
namespace cache {

class CachePushStreamParent final : public PCachePushStreamParent
{
public:
  static CachePushStreamParent*
  Create();

  ~CachePushStreamParent();

  already_AddRefed<nsIInputStream>
  TakeReader();

private:
  CachePushStreamParent(nsIAsyncInputStream* aReader,
                        nsIAsyncOutputStream* aWriter);

  // PCachePushStreamParent methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  virtual bool
  RecvBuffer(const nsCString& aBuffer) override;

  virtual bool
  RecvClose(const nsresult& aRv) override;

  nsCOMPtr<nsIAsyncInputStream> mReader;
  nsCOMPtr<nsIAsyncOutputStream> mWriter;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CachePushStreamParent_h
