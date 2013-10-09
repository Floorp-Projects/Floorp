/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "gfx2DGlue.h"
#include "gfxImageSurface.h"
#include "gfxTypes.h"
#include "ImageContainer.h"
#include "mozilla/layers/Compositor.h"
#include "LayerTestUtils.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/BasicCompositor.h"
#ifdef XP_WIN
#include "mozilla/layers/CompositorD3D11.h"
#include "mozilla/layers/CompositorD3D9.h"
#endif

namespace mozilla {
namespace layers {

Compositor*
CreateCompositor(LayersBackend aBackend, nsIWidget* aWidget)
{
  switch (aBackend) {
    case LAYERS_BASIC: return new BasicCompositor(aWidget);
    case LAYERS_OPENGL: return new CompositorOGL(aWidget, 800, 600, false);
#ifdef XP_WIN
    case LAYERS_D3D11: return new CompositorD3D11(aWidget);
    case LAYERS_D3D9: return new CompositorD3D9(aWidget);
#endif
  }
  return nullptr;
}

// fills the surface with values betwee 0 and 100.
void
SetupSurface(gfx::DataSourceSurface* surface)
{
  int bpp = gfx::BytesPerPixel(surface->GetFormat());
  int stride = surface->Stride();
  uint8_t val = 0;
  uint8_t* data = surface->GetData();
  for (int y = 0; y < surface->GetSize().height; ++y) {
    for (int x = 0; x < surface->GetSize().width; ++x) {
      for (int b = 0; b < bpp; ++b) {
        data[y*stride + x*bpp + b] = val;
        if (val == 100) {
          val = 0;
        } else {
          ++val;
        }
      }
    }
  }
}

// fills the surface with values betwee 0 and 100.
void
SetupSurface(gfxImageSurface* surface)
{
  int bpp = gfxASurface::BytePerPixelFromFormat(surface->Format());
  int stride = surface->Stride();
  uint8_t val = 0;
  uint8_t* data = surface->Data();
  for (int y = 0; y < surface->Height(); ++y) {
    for (int x = 0; x < surface->Width(); ++x) {
      for (int b = 0; b < bpp; ++b) {
        data[y*stride + x*bpp + b] = val;
        if (val == 100) {
          val = 0;
        } else {
          ++val;
        }
      }
    }
  }
}


// return true if two surfaces contain the same data
void
AssertSurfacesEqual(gfx::DataSourceSurface* surface1,
                    gfx::DataSourceSurface* surface2)
{
  ASSERT_EQ(surface1->GetSize(), surface2->GetSize());
  ASSERT_EQ(surface1->GetFormat(), surface2->GetFormat());

  uint8_t* data1 = surface1->GetData();
  uint8_t* data2 = surface2->GetData();
  int stride1 = surface1->Stride();
  int stride2 = surface2->Stride();
  int bpp = gfx::BytesPerPixel(surface1->GetFormat());

  for (int y = 0; y < surface1->GetSize().height; ++y) {
    for (int x = 0; x < surface1->GetSize().width; ++x) {
      for (int b = 0; b < bpp; ++b) {
        ASSERT_EQ(data1[y*stride1 + x*bpp + b],
                  data2[y*stride2 + x*bpp + b]);
      }
    }
  }
}

// return true if two surfaces contain the same data
void
AssertSurfacesEqual(gfxImageSurface* surface1,
                    gfxImageSurface* surface2)
{
  ASSERT_EQ(surface1->GetSize(), surface2->GetSize());
  ASSERT_EQ(surface1->Format(), surface2->Format());

  uint8_t* data1 = surface1->Data();
  uint8_t* data2 = surface2->Data();
  int stride1 = surface1->Stride();
  int stride2 = surface2->Stride();
  int bpp = gfxASurface::BytePerPixelFromFormat(surface1->Format());

  for (int y = 0; y < surface1->Height(); ++y) {
    for (int x = 0; x < surface1->Width(); ++x) {
      for (int b = 0; b < bpp; ++b) {
        ASSERT_EQ(data1[y*stride1 + x*bpp + b],
                  data2[y*stride2 + x*bpp + b]);
      }
    }
  }
}

// Same as above, for YCbCr surfaces
void
AssertYCbCrSurfacesEqual(PlanarYCbCrData* surface1,
                         PlanarYCbCrData* surface2)
{
  ASSERT_EQ(surface1->mYSize, surface2->mYSize);
  ASSERT_EQ(surface1->mCbCrSize, surface2->mCbCrSize);
  ASSERT_EQ(surface1->mStereoMode, surface2->mStereoMode);
  ASSERT_EQ(surface1->mPicSize, surface2->mPicSize);

  for (int y = 0; y < surface1->mYSize.height; ++y) {
    for (int x = 0; x < surface1->mYSize.width; ++x) {
      ASSERT_EQ(surface1->mYChannel[y*surface1->mYStride + x*(1+surface1->mYSkip)],
                surface2->mYChannel[y*surface2->mYStride + x*(1+surface2->mYSkip)]);
    }
  }
  for (int y = 0; y < surface1->mCbCrSize.height; ++y) {
    for (int x = 0; x < surface1->mCbCrSize.width; ++x) {
      ASSERT_EQ(surface1->mCbChannel[y*surface1->mCbCrStride + x*(1+surface1->mCbSkip)],
                surface2->mCbChannel[y*surface2->mCbCrStride + x*(1+surface2->mCbSkip)]);
      ASSERT_EQ(surface1->mCrChannel[y*surface1->mCbCrStride + x*(1+surface1->mCrSkip)],
                surface2->mCrChannel[y*surface2->mCbCrStride + x*(1+surface2->mCrSkip)]);
    }
  }
}

} // layers
} // mozilla
