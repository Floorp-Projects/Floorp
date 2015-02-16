
#include "GLImages.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "GLImages.h"
#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "GLLibraryEGL.h"

using namespace mozilla;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

static nsRefPtr<GLContext> sSnapshotContext;

EGLImageImage::~EGLImageImage()
{
  if (!mData.mOwns) {
    return;
  }

  if (mData.mImage) {
    sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mData.mImage);
    mData.mImage = nullptr;
  }

  if (mData.mSync) {
    sEGLLibrary.fDestroySync(EGL_DISPLAY(), mData.mSync);
    mData.mSync = nullptr;
  }
}

TemporaryRef<gfx::SourceSurface>
GLImage::GetAsSourceSurface()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread");

  if (!sSnapshotContext) {
    sSnapshotContext = GLContextProvider::CreateHeadless(false);
    if (!sSnapshotContext) {
      NS_WARNING("Failed to create snapshot GLContext");
      return nullptr;
    }
  }

  sSnapshotContext->MakeCurrent();
  ScopedTexture scopedTex(sSnapshotContext);
  ScopedBindTexture boundTex(sSnapshotContext, scopedTex.Texture());

  gfx::IntSize size = GetSize();
  sSnapshotContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA,
                                size.width, size.height, 0,
                                LOCAL_GL_RGBA,
                                LOCAL_GL_UNSIGNED_BYTE,
                                nullptr);

  ScopedFramebufferForTexture fb(sSnapshotContext, scopedTex.Texture());

  GLBlitHelper helper(sSnapshotContext);

  if (!helper.BlitImageToFramebuffer(this, size, fb.FB(), true)) {
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> source =
        gfx::Factory::CreateDataSourceSurface(size, gfx::SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!source)) {
    return nullptr;
  }

  ScopedBindFramebuffer bind(sSnapshotContext, fb.FB());
  ReadPixelsIntoDataSurface(sSnapshotContext, source);
  return source.forget();
}

} // layers
} // mozilla
