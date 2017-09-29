#ifdef MOZ_WIDGET_ANDROID

#include "AndroidSurfaceTexture.h"

using namespace mozilla;

namespace mozilla {
namespace gl {

void
AndroidSurfaceTexture::GetTransformMatrix(java::sdk::SurfaceTexture::Param surfaceTexture,
                                          gfx::Matrix4x4* outMatrix)
{
  JNIEnv* const env = jni::GetEnvForThread();

  auto jarray = jni::FloatArray::LocalRef::Adopt(env, env->NewFloatArray(16));
  surfaceTexture->GetTransformMatrix(jarray);

  jfloat* array = env->GetFloatArrayElements(jarray.Get(), nullptr);

  memcpy(&(outMatrix->_11), array, sizeof(float)*16);

  env->ReleaseFloatArrayElements(jarray.Get(), array, 0);
}

} // gl
} // mozilla
#endif // MOZ_WIDGET_ANDROID
