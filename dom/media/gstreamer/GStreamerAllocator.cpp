#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "GStreamerAllocator.h"

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include "GStreamerLoader.h"

using namespace mozilla::layers;

namespace mozilla {

typedef struct
{
  GstAllocator parent;
  GStreamerReader *reader;
} MozGfxMemoryAllocator;

typedef struct
{
  GstAllocatorClass parent;
} MozGfxMemoryAllocatorClass;

typedef struct
{
  GstMemory memory;
  PlanarYCbCrImage* image;
  guint8* data;
} MozGfxMemory;

typedef struct
{
  GstMeta meta;
} MozGfxMeta;

typedef struct
{
  GstVideoBufferPoolClass parent_class;
} MozGfxBufferPoolClass;

typedef struct
{
  GstVideoBufferPool pool;
} MozGfxBufferPool;

// working around GTK+ bug https://bugzilla.gnome.org/show_bug.cgi?id=723899
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
G_DEFINE_TYPE(MozGfxMemoryAllocator, moz_gfx_memory_allocator, GST_TYPE_ALLOCATOR);
G_DEFINE_TYPE(MozGfxBufferPool, moz_gfx_buffer_pool, GST_TYPE_VIDEO_BUFFER_POOL);
#pragma GCC diagnostic pop

void
moz_gfx_memory_reset(MozGfxMemory *mem)
{
  if (mem->image)
    mem->image->Release();

  ImageContainer* container = ((MozGfxMemoryAllocator*) mem->memory.allocator)->reader->GetImageContainer();
  mem->image = reinterpret_cast<PlanarYCbCrImage*>(container->CreateImage(ImageFormat::PLANAR_YCBCR).take());
  mem->data = mem->image->AllocateAndGetNewBuffer(mem->memory.size);
}

static GstMemory*
moz_gfx_memory_allocator_alloc(GstAllocator* aAllocator, gsize aSize,
    GstAllocationParams* aParams)
{
  MozGfxMemory* mem = g_slice_new (MozGfxMemory);
  gsize maxsize = aSize + aParams->prefix + aParams->padding;
  gst_memory_init(GST_MEMORY_CAST (mem),
                  (GstMemoryFlags)aParams->flags,
                  aAllocator, NULL, maxsize, aParams->align,
                  aParams->prefix, aSize);
  mem->image = NULL;
  moz_gfx_memory_reset(mem);

  return (GstMemory *) mem;
}

static void
moz_gfx_memory_allocator_free (GstAllocator * allocator, GstMemory * gmem)
{
  MozGfxMemory *mem = (MozGfxMemory *) gmem;

  if (mem->memory.parent)
    goto sub_mem;

  if (mem->image)
    mem->image->Release();

sub_mem:
  g_slice_free (MozGfxMemory, mem);
}

static gpointer
moz_gfx_memory_map (MozGfxMemory * mem, gsize maxsize, GstMapFlags flags)
{
  // check that the allocation didn't fail
  if (mem->data == nullptr)
    return nullptr;

  return mem->data + mem->memory.offset;
}

static gboolean
moz_gfx_memory_unmap (MozGfxMemory * mem)
{
  return TRUE;
}

static MozGfxMemory *
moz_gfx_memory_share (MozGfxMemory * mem, gssize offset, gsize size)
{
  MozGfxMemory *sub;
  GstMemory *parent;

  /* find the real parent */
  if ((parent = mem->memory.parent) == NULL)
    parent = (GstMemory *) mem;

  if (size == (gsize) -1)
    size = mem->memory.size - offset;

  /* the shared memory is always readonly */
  sub = g_slice_new (MozGfxMemory);

  gst_memory_init (GST_MEMORY_CAST (sub),
      (GstMemoryFlags) (GST_MINI_OBJECT_FLAGS (parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY),
      mem->memory.allocator, &mem->memory, mem->memory.maxsize, mem->memory.align,
      mem->memory.offset + offset, size);

  sub->image = mem->image;
  sub->data = mem->data;

  return sub;
}

static void
moz_gfx_memory_allocator_class_init (MozGfxMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = moz_gfx_memory_allocator_alloc;
  allocator_class->free = moz_gfx_memory_allocator_free;
}

static void
moz_gfx_memory_allocator_init (MozGfxMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = "moz-gfx-image";
  alloc->mem_map = (GstMemoryMapFunction) moz_gfx_memory_map;
  alloc->mem_unmap = (GstMemoryUnmapFunction) moz_gfx_memory_unmap;
  alloc->mem_share = (GstMemoryShareFunction) moz_gfx_memory_share;
  /* fallback copy and is_span */
}

void
moz_gfx_memory_allocator_set_reader(GstAllocator* aAllocator, GStreamerReader* aReader)
{
  MozGfxMemoryAllocator *allocator = (MozGfxMemoryAllocator *) aAllocator;
  allocator->reader = aReader;
}

nsRefPtr<PlanarYCbCrImage>
moz_gfx_memory_get_image(GstMemory *aMemory)
{
  NS_ASSERTION(GST_IS_MOZ_GFX_MEMORY_ALLOCATOR(aMemory->allocator), "Should be a gfx image");

  return ((MozGfxMemory *) aMemory)->image;
}

void
moz_gfx_buffer_pool_reset_buffer (GstBufferPool* aPool, GstBuffer* aBuffer)
{
  GstMemory* mem = gst_buffer_peek_memory(aBuffer, 0);

  NS_ASSERTION(GST_IS_MOZ_GFX_MEMORY_ALLOCATOR(mem->allocator), "Should be a gfx image");
  moz_gfx_memory_reset((MozGfxMemory *) mem);
  GST_BUFFER_POOL_CLASS(moz_gfx_buffer_pool_parent_class)->reset_buffer(aPool, aBuffer);
}

static void
moz_gfx_buffer_pool_class_init (MozGfxBufferPoolClass * klass)
{
  GstBufferPoolClass *pool_class = (GstBufferPoolClass *) klass;
  pool_class->reset_buffer = moz_gfx_buffer_pool_reset_buffer;
}

static void
moz_gfx_buffer_pool_init (MozGfxBufferPool * pool)
{
}

} // namespace mozilla
