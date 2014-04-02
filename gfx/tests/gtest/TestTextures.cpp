/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/Tools.h"
#include "ImageContainer.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"

using namespace mozilla;
using namespace mozilla::layers;

/*
 * This test performs the following actions:
 * - creates a surface
 * - initialize a texture client with it
 * - serilaizes the texture client
 * - deserializes the data into a texture host
 * - reads the surface from the texture host.
 *
 * The surface in the end should be equal to the inital one.
 * This test is run for different combinations of texture types and
 * image formats.
 */


// fills the surface with values betwee 0 and 100.
void SetupSurface(gfx::DataSourceSurface* surface) {
  uint8_t* data = surface->GetData();
  gfx::IntSize size = surface->GetSize();
  uint32_t stride = surface->Stride();
  int bpp = gfx::BytesPerPixel(surface->GetFormat());
  uint8_t val = 0;
  for (int y = 0; y < size.width; ++y) {
    for (int x = 0; x < size.height; ++x) {
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
void AssertSurfacesEqual(gfx::DataSourceSurface* surface1,
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

// Same as above, for YCbCr surfaces
void AssertYCbCrSurfacesEqual(PlanarYCbCrData* surface1,
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

// Run the test for a texture client and a surface
void TestTextureClientSurface(TextureClient* texture, gfx::DataSourceSurface* surface) {

  // client allocation
  ASSERT_TRUE(texture->CanExposeDrawTarget());
  texture->AllocateForSurface(surface->GetSize());
  ASSERT_TRUE(texture->IsAllocated());

  ASSERT_TRUE(texture->Lock(OPEN_READ_WRITE));
  // client painting
  RefPtr<DrawTarget> dt = texture->GetAsDrawTarget();
  dt->CopySurface(surface, IntRect(IntPoint(), surface->GetSize()), IntPoint());
  dt = nullptr;
  texture->Unlock();

  // client serialization
  SurfaceDescriptor descriptor;
  ASSERT_TRUE(texture->ToSurfaceDescriptor(descriptor));

  ASSERT_NE(descriptor.type(), SurfaceDescriptor::Tnull_t);

  // host deserialization
  RefPtr<TextureHost> host = CreateBackendIndependentTextureHost(descriptor, nullptr,
                                                                 texture->GetFlags());

  ASSERT_TRUE(host.get() != nullptr);
  ASSERT_EQ(host->GetFlags(), texture->GetFlags());

  // host read
  ASSERT_TRUE(host->Lock());
  RefPtr<mozilla::gfx::DataSourceSurface> hostDataSurface = host->GetAsSurface();
  AssertSurfacesEqual(surface, hostDataSurface);

  host->Unlock();
}

// Same as above, for YCbCr surfaces
void TestTextureClientYCbCr(TextureClient* client, PlanarYCbCrData& ycbcrData) {

  // client allocation
  ASSERT_TRUE(client->AsTextureClientYCbCr() != nullptr);
  TextureClientYCbCr* texture = client->AsTextureClientYCbCr();
  texture->AllocateForYCbCr(ycbcrData.mYSize,
                            ycbcrData.mCbCrSize,
                            ycbcrData.mStereoMode);
  ASSERT_TRUE(client->IsAllocated());

  ASSERT_TRUE(client->Lock(OPEN_READ_WRITE));
  // client painting
  texture->UpdateYCbCr(ycbcrData);
  client->Unlock();

  // client serialization
  SurfaceDescriptor descriptor;
  ASSERT_TRUE(client->ToSurfaceDescriptor(descriptor));

  ASSERT_NE(descriptor.type(), SurfaceDescriptor::Tnull_t);

  // host deserialization
  RefPtr<TextureHost> textureHost = CreateBackendIndependentTextureHost(descriptor, nullptr,
                                                                        client->GetFlags());

  RefPtr<BufferTextureHost> host = static_cast<BufferTextureHost*>(textureHost.get());

  ASSERT_TRUE(host.get() != nullptr);
  ASSERT_EQ(host->GetFlags(), client->GetFlags());

  // host read
  ASSERT_TRUE(host->Lock());

  // This will work iff the compositor is not BasicCompositor
  ASSERT_EQ(host->GetFormat(), mozilla::gfx::SurfaceFormat::YUV);

  YCbCrImageDataDeserializer yuvDeserializer(host->GetBuffer(), host->GetBufferSize());
  ASSERT_TRUE(yuvDeserializer.IsValid());
  PlanarYCbCrData data;
  data.mYChannel = yuvDeserializer.GetYData();
  data.mCbChannel = yuvDeserializer.GetCbData();
  data.mCrChannel = yuvDeserializer.GetCrData();
  data.mYStride = yuvDeserializer.GetYStride();
  data.mCbCrStride = yuvDeserializer.GetCbCrStride();
  data.mStereoMode = yuvDeserializer.GetStereoMode();
  data.mYSize = yuvDeserializer.GetYSize();
  data.mCbCrSize = yuvDeserializer.GetCbCrSize();
  data.mYSkip = 0;
  data.mCbSkip = 0;
  data.mCrSkip = 0;
  data.mPicSize = data.mYSize;
  data.mPicX = 0;
  data.mPicY = 0;

  AssertYCbCrSurfacesEqual(&ycbcrData, &data);
  host->Unlock();
}

TEST(Layers, TextureSerialization) {
  // the test is run on all the following image formats
  gfx::SurfaceFormat formats[3] = {
    gfx::SurfaceFormat::B8G8R8A8,
    gfx::SurfaceFormat::R8G8B8X8,
    gfx::SurfaceFormat::A8,
  };

  for (int f = 0; f < 3; ++f) {
    RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateDataSourceSurface(gfx::IntSize(400,300), formats[f]);
    SetupSurface(surface);
    AssertSurfacesEqual(surface, surface);

    RefPtr<TextureClient> client
      = new MemoryTextureClient(nullptr,
                                surface->GetFormat(),
                                gfx::BackendType::CAIRO,
                                TEXTURE_DEALLOCATE_CLIENT);

    TestTextureClientSurface(client, surface);

    // XXX - Test more texture client types.
  }
}

TEST(Layers, TextureYCbCrSerialization) {
  RefPtr<gfx::DataSourceSurface> ySurface = gfx::Factory::CreateDataSourceSurface(
    IntSize(400,300), gfx::SurfaceFormat::A8);
  RefPtr<gfx::DataSourceSurface> cbSurface = gfx::Factory::CreateDataSourceSurface(
    IntSize(200,150), gfx::SurfaceFormat::A8);
  RefPtr<gfx::DataSourceSurface> crSurface = gfx::Factory::CreateDataSourceSurface(
    IntSize(200,150), gfx::SurfaceFormat::A8);

  SetupSurface(ySurface.get());
  SetupSurface(cbSurface.get());
  SetupSurface(crSurface.get());

  PlanarYCbCrData clientData;
  clientData.mYChannel = ySurface->GetData();
  clientData.mCbChannel = cbSurface->GetData();
  clientData.mCrChannel = crSurface->GetData();
  clientData.mYSize = ySurface->GetSize();
  clientData.mPicSize = ySurface->GetSize();
  clientData.mCbCrSize = cbSurface->GetSize();
  clientData.mYStride = ySurface->Stride();
  clientData.mCbCrStride = cbSurface->Stride();
  clientData.mStereoMode = StereoMode::MONO;
  clientData.mYSkip = 0;
  clientData.mCbSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mPicX = 0;
  clientData.mPicX = 0;

  RefPtr<TextureClient> client
    = new MemoryTextureClient(nullptr,
                              mozilla::gfx::SurfaceFormat::YUV,
                              gfx::BackendType::CAIRO,
                              TEXTURE_DEALLOCATE_CLIENT);

  TestTextureClientYCbCr(client, clientData);

  // XXX - Test more texture client types.
}
