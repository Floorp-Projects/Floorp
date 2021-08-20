/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientWebGLExtensions.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ClientWebGLExtensionBase)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ClientWebGLExtensionBase, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ClientWebGLExtensionBase, Release)

DEFINE_WEBGL_EXTENSION_GOOP(ANGLE_instanced_arrays,
                            WebGLExtensionInstancedArrays)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_blend_minmax, WebGLExtensionBlendMinMax)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_color_buffer_float,
                            WebGLExtensionEXTColorBufferFloat)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_color_buffer_half_float,
                            WebGLExtensionColorBufferHalfFloat)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_float_blend, WebGLExtensionFloatBlend)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_frag_depth, WebGLExtensionFragDepth)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_sRGB, WebGLExtensionSRGB)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_shader_texture_lod,
                            WebGLExtensionShaderTextureLod)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_texture_filter_anisotropic,
                            WebGLExtensionTextureFilterAnisotropic)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_texture_norm16, WebGLExtensionTextureNorm16)
DEFINE_WEBGL_EXTENSION_GOOP(MOZ_debug, WebGLExtensionMOZDebug)
DEFINE_WEBGL_EXTENSION_GOOP(OES_draw_buffers_indexed,
                            WebGLExtensionDrawBuffersIndexed)
DEFINE_WEBGL_EXTENSION_GOOP(OES_element_index_uint,
                            WebGLExtensionElementIndexUint)
DEFINE_WEBGL_EXTENSION_GOOP(OES_fbo_render_mipmap,
                            WebGLExtensionFBORenderMipmap)
DEFINE_WEBGL_EXTENSION_GOOP(OES_standard_derivatives,
                            WebGLExtensionStandardDerivatives)
DEFINE_WEBGL_EXTENSION_GOOP(OES_texture_float, WebGLExtensionTextureFloat)
DEFINE_WEBGL_EXTENSION_GOOP(OES_texture_float_linear,
                            WebGLExtensionTextureFloatLinear)
DEFINE_WEBGL_EXTENSION_GOOP(OES_texture_half_float,
                            WebGLExtensionTextureHalfFloat)
DEFINE_WEBGL_EXTENSION_GOOP(OES_texture_half_float_linear,
                            WebGLExtensionTextureHalfFloatLinear)
DEFINE_WEBGL_EXTENSION_GOOP(OES_vertex_array_object, WebGLExtensionVertexArray)
DEFINE_WEBGL_EXTENSION_GOOP(OVR_multiview2, WebGLExtensionMultiview)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_color_buffer_float,
                            WebGLExtensionColorBufferFloat)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_debug_renderer_info,
                            WebGLExtensionDebugRendererInfo)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_debug_shaders, WebGLExtensionDebugShaders)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_depth_texture, WebGLExtensionDepthTexture)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_draw_buffers, WebGLExtensionDrawBuffers)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_explicit_present,
                            WebGLExtensionExplicitPresent)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_lose_context, WebGLExtensionLoseContext)

// --------------

JSObject* ClientWebGLExtensionDisjointTimerQuery::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::EXT_disjoint_timer_query_Binding::Wrap(cx, this, givenProto);
}

ClientWebGLExtensionDisjointTimerQuery::ClientWebGLExtensionDisjointTimerQuery(
    ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
  auto& state = webgl.State();
  // Add slot for new key:
  (void)state.mCurrentQueryByTarget[LOCAL_GL_TIME_ELAPSED];
}

// --------------
// Compressed textures

void ClientWebGLContext::AddCompressedFormat(const GLenum format) {
  auto& state = State();
  state.mCompressedTextureFormats.push_back(format);
}

// -

JSObject* ClientWebGLExtensionCompressedTextureASTC::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_astc_Binding::Wrap(cx, this, givenProto);
}

ClientWebGLExtensionCompressedTextureASTC::
    ClientWebGLExtensionCompressedTextureASTC(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);

  _(COMPRESSED_RGBA_ASTC_4x4_KHR)
  _(COMPRESSED_RGBA_ASTC_5x4_KHR)
  _(COMPRESSED_RGBA_ASTC_5x5_KHR)
  _(COMPRESSED_RGBA_ASTC_6x5_KHR)
  _(COMPRESSED_RGBA_ASTC_6x6_KHR)
  _(COMPRESSED_RGBA_ASTC_8x5_KHR)
  _(COMPRESSED_RGBA_ASTC_8x6_KHR)
  _(COMPRESSED_RGBA_ASTC_8x8_KHR)
  _(COMPRESSED_RGBA_ASTC_10x5_KHR)
  _(COMPRESSED_RGBA_ASTC_10x6_KHR)
  _(COMPRESSED_RGBA_ASTC_10x8_KHR)
  _(COMPRESSED_RGBA_ASTC_10x10_KHR)
  _(COMPRESSED_RGBA_ASTC_12x10_KHR)
  _(COMPRESSED_RGBA_ASTC_12x12_KHR)

  _(COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR)
  _(COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)

#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureBPTC::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::EXT_texture_compression_bptc_Binding::Wrap(cx, this, givenProto);
}

ClientWebGLExtensionCompressedTextureBPTC::
    ClientWebGLExtensionCompressedTextureBPTC(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_RGBA_BPTC_UNORM)
  _(COMPRESSED_SRGB_ALPHA_BPTC_UNORM)
  _(COMPRESSED_RGB_BPTC_SIGNED_FLOAT)
  _(COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureRGTC::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::EXT_texture_compression_rgtc_Binding::Wrap(cx, this, givenProto);
}

ClientWebGLExtensionCompressedTextureRGTC::
    ClientWebGLExtensionCompressedTextureRGTC(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_RED_RGTC1)
  _(COMPRESSED_SIGNED_RED_RGTC1)
  _(COMPRESSED_RG_RGTC2)
  _(COMPRESSED_SIGNED_RG_RGTC2)
#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureES3::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_etc_Binding::Wrap(cx, this, givenProto);
}
ClientWebGLExtensionCompressedTextureES3::
    ClientWebGLExtensionCompressedTextureES3(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_R11_EAC)
  _(COMPRESSED_SIGNED_R11_EAC)
  _(COMPRESSED_RG11_EAC)
  _(COMPRESSED_SIGNED_RG11_EAC)
  _(COMPRESSED_RGB8_ETC2)
  _(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
  _(COMPRESSED_RGBA8_ETC2_EAC)

  // sRGB support is manadatory in GL 4.3 and GL ES 3.0, which are the only
  // versions to support ETC2.
  _(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
  _(COMPRESSED_SRGB8_ETC2)
  _(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureETC1::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_etc1_Binding::Wrap(cx, this, givenProto);
}
ClientWebGLExtensionCompressedTextureETC1::
    ClientWebGLExtensionCompressedTextureETC1(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
  webgl.AddCompressedFormat(LOCAL_GL_ETC1_RGB8_OES);
}

// -

JSObject* ClientWebGLExtensionCompressedTexturePVRTC::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_pvrtc_Binding::Wrap(cx, this,
                                                           givenProto);
}
ClientWebGLExtensionCompressedTexturePVRTC::
    ClientWebGLExtensionCompressedTexturePVRTC(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_RGB_PVRTC_4BPPV1)
  _(COMPRESSED_RGB_PVRTC_2BPPV1)
  _(COMPRESSED_RGBA_PVRTC_4BPPV1)
  _(COMPRESSED_RGBA_PVRTC_2BPPV1)
#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureS3TC::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_s3tc_Binding::Wrap(cx, this, givenProto);
}
ClientWebGLExtensionCompressedTextureS3TC::
    ClientWebGLExtensionCompressedTextureS3TC(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_RGB_S3TC_DXT1_EXT)
  _(COMPRESSED_RGBA_S3TC_DXT1_EXT)
  _(COMPRESSED_RGBA_S3TC_DXT3_EXT)
  _(COMPRESSED_RGBA_S3TC_DXT5_EXT)
#undef _
}

// -

JSObject* ClientWebGLExtensionCompressedTextureS3TC_SRGB::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> givenProto) {
  return dom::WEBGL_compressed_texture_s3tc_srgb_Binding::Wrap(cx, this,
                                                               givenProto);
}

ClientWebGLExtensionCompressedTextureS3TC_SRGB::
    ClientWebGLExtensionCompressedTextureS3TC_SRGB(ClientWebGLContext& webgl)
    : ClientWebGLExtensionBase(webgl) {
#define _(X) webgl.AddCompressedFormat(LOCAL_GL_##X);
  _(COMPRESSED_SRGB_S3TC_DXT1_EXT)
  _(COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT)
  _(COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT)
  _(COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
#undef _
}

}  // namespace mozilla
