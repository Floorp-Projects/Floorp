/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GStreamerAllocator_h_)
#define GStreamerAllocator_h_

#include "GStreamerReader.h"

#define GST_TYPE_MOZ_GFX_MEMORY_ALLOCATOR   (moz_gfx_memory_allocator_get_type())
#define GST_IS_MOZ_GFX_MEMORY_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MOZ_GFX_MEMORY_ALLOCATOR))
#define GST_TYPE_MOZ_GFX_BUFFER_POOL   (moz_gfx_buffer_pool_get_type())
#define GST_IS_MOZ_GFX_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MOZ_GFX_BUFFER_POOL))

namespace mozilla {

GType moz_gfx_memory_allocator_get_type();
void moz_gfx_memory_allocator_set_reader(GstAllocator *aAllocator, GStreamerReader* aReader);
nsRefPtr<layers::PlanarYCbCrImage> moz_gfx_memory_get_image(GstMemory *aMemory);

GType moz_gfx_buffer_pool_get_type();

} // namespace mozilla

#endif
