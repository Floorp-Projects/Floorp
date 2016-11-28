/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/RefPtr.h"
#include "gfx2DGlue.h"
#include "gfxImageSurface.h"
#include "gfxTypes.h"
#include "ImageContainer.h"
#include "mozilla/layers/ImageDataSerializer.h"

using namespace mozilla;
using namespace mozilla::gfx;
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

namespace mozilla {
namespace layers {

// fills the surface with values betwee 0 and 100.
void SetupSurface(gfxImageSurface* surface) {
  int bpp = gfxASurface::BytePerPixelFromFormat(surface->Format());
  int stride = surface->Stride();
  uint8_t val = 0;
  uint8_t* data = surface->Data();
  for (int y = 0; y < surface->Height(); ++y) {
    for (int x = 0; x < surface->Height(); ++x) {
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
void AssertSurfacesEqual(gfxImageSurface* surface1,
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

void AssertSurfacesEqual(SourceSurface* surface1,
                         SourceSurface* surface2)
{
  ASSERT_EQ(surface1->GetSize(), surface2->GetSize());
  ASSERT_EQ(surface1->GetFormat(), surface2->GetFormat());

  RefPtr<DataSourceSurface> dataSurface1 = surface1->GetDataSurface();
  RefPtr<DataSourceSurface> dataSurface2 = surface2->GetDataSurface();
  DataSourceSurface::MappedSurface map1;
  DataSourceSurface::MappedSurface map2;
  if (!dataSurface1->Map(DataSourceSurface::READ, &map1)) {
    return;
  }
  if (!dataSurface2->Map(DataSourceSurface::READ, &map2)) {
    dataSurface1->Unmap();
    return;
  }
  uint8_t* data1 = map1.mData;
  uint8_t* data2 = map2.mData;
  int stride1 = map1.mStride;
  int stride2 = map2.mStride;
  int bpp = BytesPerPixel(surface1->GetFormat());
  int width = surface1->GetSize().width;
  int height = surface1->GetSize().height;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int b = 0; b < bpp; ++b) {
        ASSERT_EQ(data1[y*stride1 + x*bpp + b],
                  data2[y*stride2 + x*bpp + b]);
      }
    }
  }

  dataSurface1->Unmap();
  dataSurface2->Unmap();
}

// Run the test for a texture client and a surface
void TestTextureClientSurface(TextureClient* texture, gfxImageSurface* surface) {

  // client allocation
  ASSERT_TRUE(texture->CanExposeDrawTarget());

  ASSERT_TRUE(texture->Lock(OpenMode::OPEN_READ_WRITE));
  // client painting
  RefPtr<DrawTarget> dt = texture->BorrowDrawTarget();
  RefPtr<SourceSurface> source =
    gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, surface);
  dt->CopySurface(source, IntRect(IntPoint(), source->GetSize()), IntPoint());

  RefPtr<SourceSurface> snapshot = dt->Snapshot();

  AssertSurfacesEqual(snapshot, source);

  dt = nullptr; // drop reference before calling Unlock()
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

  // XXX - this can fail because lock tries to upload the texture but it needs a
  // Compositor to do that. We could add a DummyComposior for testing but I am
  // not sure it'll be worth it. Maybe always test against a BasicCompositor,
  // but the latter needs a widget...
  if (host->Lock()) {
    RefPtr<mozilla::gfx::DataSourceSurface> hostDataSurface = host->GetAsSurface();

    RefPtr<gfxImageSurface> hostSurface =
      new gfxImageSurface(hostDataSurface->GetData(),
                          hostDataSurface->GetSize(),
                          hostDataSurface->Stride(),
                          SurfaceFormatToImageFormat(hostDataSurface->GetFormat()));
    AssertSurfacesEqual(surface, hostSurface.get());
    host->Unlock();
  }
}

// Same as above, for YCbCr surfaces
void TestTextureClientYCbCr(TextureClient* client, PlanarYCbCrData& ycbcrData) {
  client->Lock(OpenMode::OPEN_READ_WRITE);
  UpdateYCbCrTextureClient(client, ycbcrData);
  client->Unlock();

  // client serialization
  SurfaceDescriptor descriptor;
  ASSERT_TRUE(client->ToSurfaceDescriptor(descriptor));

  ASSERT_EQ(descriptor.type(), SurfaceDescriptor::TSurfaceDescriptorBuffer);
  auto bufferDesc = descriptor.get_SurfaceDescriptorBuffer();
  ASSERT_EQ(bufferDesc.desc().type(), BufferDescriptor::TYCbCrDescriptor);
  auto ycbcrDesc = bufferDesc.desc().get_YCbCrDescriptor();
  ASSERT_EQ(ycbcrDesc.ySize(), ycbcrData.mYSize);
  ASSERT_EQ(ycbcrDesc.cbCrSize(), ycbcrData.mCbCrSize);
  ASSERT_EQ(ycbcrDesc.stereoMode(), ycbcrData.mStereoMode);

  // host deserialization
  RefPtr<TextureHost> textureHost = CreateBackendIndependentTextureHost(descriptor, nullptr,
                                                                        client->GetFlags());

  RefPtr<BufferTextureHost> host = static_cast<BufferTextureHost*>(textureHost.get());

  ASSERT_TRUE(host.get() != nullptr);
  ASSERT_EQ(host->GetFlags(), client->GetFlags());

  // host read

  if (host->Lock()) {
    // This will work iff the compositor is not BasicCompositor
    ASSERT_EQ(host->GetFormat(), mozilla::gfx::SurfaceFormat::YUV);
    host->Unlock();
  }
}

} // namespace layers
} // namespace mozilla

TEST(Layers, TextureSerialization) {
  // the test is run on all the following image formats
  gfxImageFormat formats[3] = {
    SurfaceFormat::A8R8G8B8_UINT32,
    SurfaceFormat::X8R8G8B8_UINT32,
    SurfaceFormat::A8,
  };

  for (int f = 0; f < 3; ++f) {
    RefPtr<gfxImageSurface> surface = new gfxImageSurface(IntSize(400,300), formats[f]);
    SetupSurface(surface.get());
    AssertSurfacesEqual(surface, surface);

    auto texData = BufferTextureData::Create(surface->GetSize(),
      gfx::ImageFormatToSurfaceFormat(surface->Format()),
      gfx::BackendType::CAIRO, LayersBackend::LAYERS_NONE,
      TextureFlags::DEALLOCATE_CLIENT, ALLOC_DEFAULT, nullptr
    );
    ASSERT_TRUE(!!texData);

    RefPtr<TextureClient> client = new TextureClient(
      texData, TextureFlags::DEALLOCATE_CLIENT, nullptr
    );

    TestTextureClientSurface(client, surface);

    // XXX - Test more texture client types.
  }
}

TEST(Layers, TextureYCbCrSerialization) {
  RefPtr<gfxImageSurface> ySurface = new gfxImageSurface(IntSize(400,300), SurfaceFormat::A8);
  RefPtr<gfxImageSurface> cbSurface = new gfxImageSurface(IntSize(200,150), SurfaceFormat::A8);
  RefPtr<gfxImageSurface> crSurface = new gfxImageSurface(IntSize(200,150), SurfaceFormat::A8);
  SetupSurface(ySurface.get());
  SetupSurface(cbSurface.get());
  SetupSurface(crSurface.get());

  PlanarYCbCrData clientData;
  clientData.mYChannel = ySurface->Data();
  clientData.mCbChannel = cbSurface->Data();
  clientData.mCrChannel = crSurface->Data();
  clientData.mYSize = ySurface->GetSize();
  clientData.mPicSize = ySurface->GetSize();
  clientData.mCbCrSize = cbSurface->GetSize();
  clientData.mYStride = ySurface->Stride();
  clientData.mCbCrStride = cbSurface->Stride();
  clientData.mStereoMode = StereoMode::MONO;
  clientData.mYUVColorSpace = YUVColorSpace::BT601;
  clientData.mYSkip = 0;
  clientData.mCbSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mPicX = 0;
  clientData.mPicX = 0;

  ImageBridgeChild::InitSameProcess();

  RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton();
  static int retry = 5;
  while(!imageBridge->IPCOpen() && retry) {
    // IPDL connection takes time especially in slow testing environment, like
    // VM machines. Here we added retry mechanism to wait for IPDL connnection.
#ifdef XP_WIN
    Sleep(1);
#else
    sleep(1);
#endif
    retry--;
  }

  // Skip this testing if IPDL connection is not ready
  if (!retry && !imageBridge->IPCOpen()) {
    return;
  }

  RefPtr<TextureClient> client = TextureClient::CreateForYCbCr(imageBridge, clientData.mYSize, clientData.mCbCrSize,
                                                               StereoMode::MONO, YUVColorSpace::BT601,
                                                               TextureFlags::DEALLOCATE_CLIENT);

  TestTextureClientYCbCr(client, clientData);

  // XXX - Test more texture client types.
}
