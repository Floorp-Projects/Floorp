#include "GLManager.h"
#include "LayerManagerOGL.h"
#include "CompositorOGL.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "GLContext.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class GLManagerLayerManager : public GLManager
{
public:
  GLManagerLayerManager(LayerManagerOGL* aManager)
    : mImpl(aManager)
  {}

  virtual GLContext* gl() const MOZ_OVERRIDE
  {
    return mImpl->gl();
  }

  virtual ShaderProgramOGL* GetProgram(ShaderProgramType aType) MOZ_OVERRIDE
  {
    return mImpl->GetProgram(aType);
  }

  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg) MOZ_OVERRIDE
  {
    mImpl->BindAndDrawQuad(aProg);
  }

private:
  nsRefPtr<LayerManagerOGL> mImpl;
};

class GLManagerCompositor : public GLManager
{
public:
  GLManagerCompositor(CompositorOGL* aCompositor)
    : mImpl(aCompositor)
  {}

  virtual GLContext* gl() const MOZ_OVERRIDE
  {
    return mImpl->gl();
  }

  virtual ShaderProgramOGL* GetProgram(gl::ShaderProgramType aType) MOZ_OVERRIDE
  {
    return mImpl->GetProgram(aType);
  }

  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg) MOZ_OVERRIDE
  {
    mImpl->BindAndDrawQuad(aProg);
  }

private:
  RefPtr<CompositorOGL> mImpl;
};

/* static */ GLManager*
GLManager::CreateGLManager(LayerManager* aManager)
{
  if (!aManager) {
    return nullptr;
  } else if (aManager->GetBackendType() == LAYERS_OPENGL) {
    return new GLManagerLayerManager(static_cast<LayerManagerOGL*>(aManager));
  }
  if (aManager->GetBackendType() == LAYERS_NONE) {
    if (Compositor::GetBackend() == LAYERS_OPENGL) {
      return new GLManagerCompositor(static_cast<CompositorOGL*>(
        static_cast<LayerManagerComposite*>(aManager)->GetCompositor()));
    } else {
      return nullptr;
    }
  }

  MOZ_CRASH("Cannot create GLManager for non-GL layer manager");
  return nullptr;
}

}
}
