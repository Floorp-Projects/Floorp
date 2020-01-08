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
DEFINE_WEBGL_EXTENSION_GOOP(EXT_texture_compression_bptc,
                            WebGLExtensionCompressedTextureBPTC)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_texture_compression_rgtc,
                            WebGLExtensionCompressedTextureRGTC)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_frag_depth, WebGLExtensionFragDepth)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_sRGB, WebGLExtensionSRGB)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_shader_texture_lod,
                            WebGLExtensionShaderTextureLod)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_texture_filter_anisotropic,
                            WebGLExtensionTextureFilterAnisotropic)
DEFINE_WEBGL_EXTENSION_GOOP(EXT_disjoint_timer_query,
                            WebGLExtensionDisjointTimerQuery)
DEFINE_WEBGL_EXTENSION_GOOP(MOZ_debug, WebGLExtensionMOZDebug)
DEFINE_WEBGL_EXTENSION_GOOP(OES_element_index_uint,
                            WebGLExtensionElementIndexUint)
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
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_color_buffer_float,
                            WebGLExtensionColorBufferFloat)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_astc,
                            WebGLExtensionCompressedTextureASTC)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_etc,
                            WebGLExtensionCompressedTextureES3)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_etc1,
                            WebGLExtensionCompressedTextureETC1)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_pvrtc,
                            WebGLExtensionCompressedTexturePVRTC)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_s3tc,
                            WebGLExtensionCompressedTextureS3TC)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_compressed_texture_s3tc_srgb,
                            WebGLExtensionCompressedTextureS3TC_SRGB)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_debug_renderer_info,
                            WebGLExtensionDebugRendererInfo)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_debug_shaders, WebGLExtensionDebugShaders)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_depth_texture, WebGLExtensionDepthTexture)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_draw_buffers, WebGLExtensionDrawBuffers)
DEFINE_WEBGL_EXTENSION_GOOP(WEBGL_lose_context, WebGLExtensionLoseContext)

}  // namespace mozilla
