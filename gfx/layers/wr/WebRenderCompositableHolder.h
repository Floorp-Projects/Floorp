/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include "nsDataHashtable.h"

namespace mozilla {
namespace layers {

class CompositableHost;

class WebRenderCompositableHolder final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderCompositableHolder)

  explicit WebRenderCompositableHolder();

protected:
  virtual ~WebRenderCompositableHolder();

public:

  virtual void Destroy();

  void AddExternalImageId(uint64_t aExternalImageId, CompositableHost* aHost);
  void RemoveExternalImageId(uint64_t aExternalImageId);
  void UpdateExternalImages();

private:

  // Holds CompositableHosts that are bound to external image ids.
  nsDataHashtable<nsUint64HashKey, RefPtr<CompositableHost> > mCompositableHosts;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
