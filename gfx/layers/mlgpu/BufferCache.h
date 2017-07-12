/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_BufferCache_h
#define mozilla_gfx_layers_mlgpu_BufferCache_h

#include "mozilla/EnumeratedArray.h"
#include "nsTArray.h"
#include <deque>

namespace mozilla {
namespace layers {

class MLGBuffer;
class MLGDevice;

// Cache buffer pools based on how long ago they were last used.
class BufferCache
{
public:
  explicit BufferCache(MLGDevice* aDevice);
  ~BufferCache();

  // Get a buffer that has at least |aBytes|, or create a new one
  // if none can be re-used.
  RefPtr<MLGBuffer> GetOrCreateBuffer(size_t aBytes);

  // Rotate buffers after a frame has been completed.
  void EndFrame();

private:
  // Not RefPtr since this would create a cycle.
  MLGDevice* mDevice;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_BufferCache_h
