#ifdef MOZ_WIDGET_ANDROID

#include "GLImages.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "GLImages.h"
#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "AndroidSurfaceTexture.h"

using namespace mozilla;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

static nsRefPtr<GLContext> sSnapshotContext;

TemporaryRef<gfx::SourceSurface>
SurfaceTextureImage::GetAsSourceSurface()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  if (!sSnapshotContext) {
    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    sSnapshotContext = GLContextProvider::CreateOffscreen(gfxIntSize(16, 16), caps);

    if (!sSnapshotContext) {
      return nullptr;
    }
  }

  sSnapshotContext->MakeCurrent();
  ScopedTexture scopedTex(sSnapshotContext);
  ScopedBindTexture boundTex(sSnapshotContext, scopedTex.Texture());
  sSnapshotContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA,
                                mData.mSize.width, mData.mSize.height, 0,
                                LOCAL_GL_RGBA,
                                LOCAL_GL_UNSIGNED_BYTE,
                                nullptr);

  ScopedFramebufferForTexture fb(sSnapshotContext, scopedTex.Texture());

  GLBlitHelper helper(sSnapshotContext);

  helper.BlitImageToFramebuffer(this, mData.mSize, fb.FB(), false);
  ScopedBindFramebuffer bind(sSnapshotContext, fb.FB());

  RefPtr<gfx::DataSourceSurface> source =
        gfx::Factory::CreateDataSourceSurface(mData.mSize, gfx::SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!source)) {
    return nullptr;
  }

  ReadPixelsIntoDataSurface(sSnapshotContext, source);
  return source.forget();
}

} // layers
} // mozilla

#endif
