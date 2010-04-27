#define SHADER_GLOBAL_VARS "uniform mat4 uMatrixProj; \
uniform mat4 uLayerQuadTransform; \
uniform mat4 uLayerTransform; \
uniform vec4 uRenderTargetOffset; \
uniform float uLayerOpacity; \
\
uniform sampler2D uLayerTexture; \
uniform sampler2D uYTexture; \
uniform sampler2D uCbTexture; \
uniform sampler2D uCrTexture; \
varying vec2 vTextureCoordinate;"

static const GLchar *sVertexShader = SHADER_GLOBAL_VARS "attribute vec4 aVertex; \
void main() \
{ \
    vec4 finalPosition = aVertex; \
    finalPosition = uLayerQuadTransform * finalPosition; \
    finalPosition = uLayerTransform * finalPosition; \
    finalPosition = finalPosition - uRenderTargetOffset; \
    finalPosition = uMatrixProj * finalPosition; \
    gl_Position = finalPosition; \
    vTextureCoordinate = vec2(aVertex); \
    }";

static const GLchar *sRGBLayerPS = SHADER_GLOBAL_VARS "void main() \
{ \
gl_FragColor = texture2D(uLayerTexture, vTextureCoordinate) * uLayerOpacity; \
}";

static const GLchar *sYUVLayerPS = SHADER_GLOBAL_VARS "void main() \
{ \
    vec4 yuv; \
    vec4 color; \
    yuv.r = texture2D(uCrTexture, vTextureCoordinate).r - 0.5; \
    yuv.g = texture2D(uYTexture, vTextureCoordinate).r - 0.0625; \
    yuv.b = texture2D(uCbTexture, vTextureCoordinate).r - 0.5; \
    color.r = yuv.g * 1.164 + yuv.r * 1.596; \
    color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b; \
    color.b = yuv.g * 1.164 + yuv.b * 2.018; \
    color.a = 1.0; \
    gl_FragColor = color * uLayerOpacity; \
}";
