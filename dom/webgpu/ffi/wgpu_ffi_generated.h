/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.15.0 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --lockfile Cargo.lock --crate wgpu_bindings -o dom/webgpu/ffi/wgpu_ffi_generated.h`
 */

struct WGPUByteBuf;
typedef uint64_t WGPUNonZeroU64;
typedef uint64_t WGPUOption_BufferSize;
typedef uint32_t WGPUOption_NonZeroU32;
typedef uint8_t WGPUOption_NonZeroU8;
typedef uint64_t WGPUOption_AdapterId;
typedef uint64_t WGPUOption_BufferId;
typedef uint64_t WGPUOption_PipelineLayoutId;
typedef uint64_t WGPUOption_SamplerId;
typedef uint64_t WGPUOption_SurfaceId;
typedef uint64_t WGPUOption_TextureViewId;


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define WGPUMAX_BIND_GROUPS 8

#define WGPUMAX_COLOR_TARGETS 4

#define WGPUMAX_MIP_LEVELS 16

#define WGPUMAX_VERTEX_BUFFERS 16

#define WGPUMAX_ANISOTROPY 16

#define WGPUSHADER_STAGE_COUNT 3

#define WGPUDESIRED_NUM_FRAMES 3

/**
 * Buffer-Texture copies must have [`bytes_per_row`] aligned to this number.
 *
 * This doesn't apply to [`Queue::write_texture`].
 *
 * [`bytes_per_row`]: TextureDataLayout::bytes_per_row
 */
#define WGPUCOPY_BYTES_PER_ROW_ALIGNMENT 256

/**
 * Alignment all push constants need
 */
#define WGPUPUSH_CONSTANT_ALIGNMENT 4

/**
 * How edges should be handled in texture addressing.
 */
enum WGPUAddressMode {
  /**
   * Clamp the value to the edge of the texture
   *
   * -0.25 -> 0.0
   * 1.25  -> 1.0
   */
  WGPUAddressMode_ClampToEdge = 0,
  /**
   * Repeat the texture in a tiling fashion
   *
   * -0.25 -> 0.75
   * 1.25 -> 0.25
   */
  WGPUAddressMode_Repeat = 1,
  /**
   * Repeat the texture, mirroring it every repeat
   *
   * -0.25 -> 0.25
   * 1.25 -> 0.75
   */
  WGPUAddressMode_MirrorRepeat = 2,
  /**
   * Clamp the value to the border of the texture
   * Requires feature [`Features::ADDRESS_MODE_CLAMP_TO_BORDER`]
   *
   * -0.25 -> border
   * 1.25 -> border
   */
  WGPUAddressMode_ClampToBorder = 3,
  /**
   * Must be last for serialization purposes
   */
  WGPUAddressMode_Sentinel,
};

/**
 * Alpha blend factor.
 *
 * Alpha blending is very complicated: see the OpenGL or Vulkan spec for more information.
 */
enum WGPUBlendFactor {
  WGPUBlendFactor_Zero = 0,
  WGPUBlendFactor_One = 1,
  WGPUBlendFactor_SrcColor = 2,
  WGPUBlendFactor_OneMinusSrcColor = 3,
  WGPUBlendFactor_SrcAlpha = 4,
  WGPUBlendFactor_OneMinusSrcAlpha = 5,
  WGPUBlendFactor_DstColor = 6,
  WGPUBlendFactor_OneMinusDstColor = 7,
  WGPUBlendFactor_DstAlpha = 8,
  WGPUBlendFactor_OneMinusDstAlpha = 9,
  WGPUBlendFactor_SrcAlphaSaturated = 10,
  WGPUBlendFactor_BlendColor = 11,
  WGPUBlendFactor_OneMinusBlendColor = 12,
  /**
   * Must be last for serialization purposes
   */
  WGPUBlendFactor_Sentinel,
};

/**
 * Alpha blend operation.
 *
 * Alpha blending is very complicated: see the OpenGL or Vulkan spec for more information.
 */
enum WGPUBlendOperation {
  WGPUBlendOperation_Add = 0,
  WGPUBlendOperation_Subtract = 1,
  WGPUBlendOperation_ReverseSubtract = 2,
  WGPUBlendOperation_Min = 3,
  WGPUBlendOperation_Max = 4,
  /**
   * Must be last for serialization purposes
   */
  WGPUBlendOperation_Sentinel,
};

enum WGPUBufferMapAsyncStatus {
  WGPUBufferMapAsyncStatus_Success,
  WGPUBufferMapAsyncStatus_Error,
  WGPUBufferMapAsyncStatus_Unknown,
  WGPUBufferMapAsyncStatus_ContextLost,
  /**
   * Must be last for serialization purposes
   */
  WGPUBufferMapAsyncStatus_Sentinel,
};

/**
 * Comparison function used for depth and stencil operations.
 */
enum WGPUCompareFunction {
  /**
   * Function never passes
   */
  WGPUCompareFunction_Never = 1,
  /**
   * Function passes if new value less than existing value
   */
  WGPUCompareFunction_Less = 2,
  /**
   * Function passes if new value is equal to existing value
   */
  WGPUCompareFunction_Equal = 3,
  /**
   * Function passes if new value is less than or equal to existing value
   */
  WGPUCompareFunction_LessEqual = 4,
  /**
   * Function passes if new value is greater than existing value
   */
  WGPUCompareFunction_Greater = 5,
  /**
   * Function passes if new value is not equal to existing value
   */
  WGPUCompareFunction_NotEqual = 6,
  /**
   * Function passes if new value is greater than or equal to existing value
   */
  WGPUCompareFunction_GreaterEqual = 7,
  /**
   * Function always passes
   */
  WGPUCompareFunction_Always = 8,
  /**
   * Must be last for serialization purposes
   */
  WGPUCompareFunction_Sentinel,
};

/**
 * Type of faces to be culled.
 */
enum WGPUCullMode {
  /**
   * No faces should be culled
   */
  WGPUCullMode_None = 0,
  /**
   * Front faces should be culled
   */
  WGPUCullMode_Front = 1,
  /**
   * Back faces should be culled
   */
  WGPUCullMode_Back = 2,
  /**
   * Must be last for serialization purposes
   */
  WGPUCullMode_Sentinel,
};

/**
 * Texel mixing mode when sampling between texels.
 */
enum WGPUFilterMode {
  /**
   * Nearest neighbor sampling.
   *
   * This creates a pixelated effect when used as a mag filter
   */
  WGPUFilterMode_Nearest = 0,
  /**
   * Linear Interpolation
   *
   * This makes textures smooth but blurry when used as a mag filter.
   */
  WGPUFilterMode_Linear = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUFilterMode_Sentinel,
};

/**
 * Winding order which classifies the "front" face.
 */
enum WGPUFrontFace {
  /**
   * Triangles with vertices in counter clockwise order are considered the front face.
   *
   * This is the default with right handed coordinate spaces.
   */
  WGPUFrontFace_Ccw = 0,
  /**
   * Triangles with vertices in clockwise order are considered the front face.
   *
   * This is the default with left handed coordinate spaces.
   */
  WGPUFrontFace_Cw = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUFrontFace_Sentinel,
};

enum WGPUHostMap {
  WGPUHostMap_Read,
  WGPUHostMap_Write,
  /**
   * Must be last for serialization purposes
   */
  WGPUHostMap_Sentinel,
};

/**
 * Format of indices used with pipeline.
 */
enum WGPUIndexFormat {
  /**
   * Indices are 16 bit unsigned integers.
   */
  WGPUIndexFormat_Uint16 = 0,
  /**
   * Indices are 32 bit unsigned integers.
   */
  WGPUIndexFormat_Uint32 = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUIndexFormat_Sentinel,
};

/**
 * Rate that determines when vertex data is advanced.
 */
enum WGPUInputStepMode {
  /**
   * Input data is advanced every vertex. This is the standard value for vertex data.
   */
  WGPUInputStepMode_Vertex = 0,
  /**
   * Input data is advanced every instance.
   */
  WGPUInputStepMode_Instance = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUInputStepMode_Sentinel,
};

/**
 * Operation to perform to the output attachment at the start of a renderpass.
 */
enum WGPULoadOp {
  /**
   * Clear the output attachment with the clear color. Clearing is faster than loading.
   */
  WGPULoadOp_Clear = 0,
  /**
   * Do not clear output attachment.
   */
  WGPULoadOp_Load = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPULoadOp_Sentinel,
};

/**
 * Type of drawing mode for polygons
 */
enum WGPUPolygonMode {
  /**
   * Polygons are filled
   */
  WGPUPolygonMode_Fill = 0,
  /**
   * Polygons are draw as line segments
   */
  WGPUPolygonMode_Line = 1,
  /**
   * Polygons are draw as points
   */
  WGPUPolygonMode_Point = 2,
  /**
   * Must be last for serialization purposes
   */
  WGPUPolygonMode_Sentinel,
};

/**
 * Power Preference when choosing a physical adapter.
 */
enum WGPUPowerPreference {
  /**
   * Adapter that uses the least possible power. This is often an integerated GPU.
   */
  WGPUPowerPreference_LowPower = 0,
  /**
   * Adapter that has the highest performance. This is often a discrete GPU.
   */
  WGPUPowerPreference_HighPerformance = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUPowerPreference_Sentinel,
};

/**
 * Primitive type the input mesh is composed of.
 */
enum WGPUPrimitiveTopology {
  /**
   * Vertex data is a list of points. Each vertex is a new point.
   */
  WGPUPrimitiveTopology_PointList = 0,
  /**
   * Vertex data is a list of lines. Each pair of vertices composes a new line.
   *
   * Vertices `0 1 2 3` create two lines `0 1` and `2 3`
   */
  WGPUPrimitiveTopology_LineList = 1,
  /**
   * Vertex data is a strip of lines. Each set of two adjacent vertices form a line.
   *
   * Vertices `0 1 2 3` create three lines `0 1`, `1 2`, and `2 3`.
   */
  WGPUPrimitiveTopology_LineStrip = 2,
  /**
   * Vertex data is a list of triangles. Each set of 3 vertices composes a new triangle.
   *
   * Vertices `0 1 2 3 4 5` create two triangles `0 1 2` and `3 4 5`
   */
  WGPUPrimitiveTopology_TriangleList = 3,
  /**
   * Vertex data is a triangle strip. Each set of three adjacent vertices form a triangle.
   *
   * Vertices `0 1 2 3 4 5` creates four triangles `0 1 2`, `2 1 3`, `3 2 4`, and `4 3 5`
   */
  WGPUPrimitiveTopology_TriangleStrip = 4,
  /**
   * Must be last for serialization purposes
   */
  WGPUPrimitiveTopology_Sentinel,
};

enum WGPURawBindingType {
  WGPURawBindingType_UniformBuffer,
  WGPURawBindingType_StorageBuffer,
  WGPURawBindingType_ReadonlyStorageBuffer,
  WGPURawBindingType_Sampler,
  WGPURawBindingType_ComparisonSampler,
  WGPURawBindingType_SampledTexture,
  WGPURawBindingType_ReadonlyStorageTexture,
  WGPURawBindingType_WriteonlyStorageTexture,
  /**
   * Must be last for serialization purposes
   */
  WGPURawBindingType_Sentinel,
};

/**
 * Operation to perform on the stencil value.
 */
enum WGPUStencilOperation {
  /**
   * Keep stencil value unchanged.
   */
  WGPUStencilOperation_Keep = 0,
  /**
   * Set stencil value to zero.
   */
  WGPUStencilOperation_Zero = 1,
  /**
   * Replace stencil value with value provided in most recent call to [`RenderPass::set_stencil_reference`].
   */
  WGPUStencilOperation_Replace = 2,
  /**
   * Bitwise inverts stencil value.
   */
  WGPUStencilOperation_Invert = 3,
  /**
   * Increments stencil value by one, clamping on overflow.
   */
  WGPUStencilOperation_IncrementClamp = 4,
  /**
   * Decrements stencil value by one, clamping on underflow.
   */
  WGPUStencilOperation_DecrementClamp = 5,
  /**
   * Increments stencil value by one, wrapping on overflow.
   */
  WGPUStencilOperation_IncrementWrap = 6,
  /**
   * Decrements stencil value by one, wrapping on underflow.
   */
  WGPUStencilOperation_DecrementWrap = 7,
  /**
   * Must be last for serialization purposes
   */
  WGPUStencilOperation_Sentinel,
};

/**
 * Operation to perform to the output attachment at the end of a renderpass.
 */
enum WGPUStoreOp {
  /**
   * Clear the render target. If you don't care about the contents of the target, this can be faster.
   */
  WGPUStoreOp_Clear = 0,
  /**
   * Store the result of the renderpass.
   */
  WGPUStoreOp_Store = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUStoreOp_Sentinel,
};

/**
 * Kind of data the texture holds.
 */
enum WGPUTextureAspect {
  /**
   * Depth, Stencil, and Color.
   */
  WGPUTextureAspect_All,
  /**
   * Stencil.
   */
  WGPUTextureAspect_StencilOnly,
  /**
   * Depth.
   */
  WGPUTextureAspect_DepthOnly,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureAspect_Sentinel,
};

/**
 * Dimensionality of a texture.
 */
enum WGPUTextureDimension {
  /**
   * 1D texture
   */
  WGPUTextureDimension_D1,
  /**
   * 2D texture
   */
  WGPUTextureDimension_D2,
  /**
   * 3D texture
   */
  WGPUTextureDimension_D3,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureDimension_Sentinel,
};

/**
 * Underlying texture data format.
 *
 * If there is a conversion in the format (such as srgb -> linear), The conversion listed is for
 * loading from texture in a shader. When writing to the texture, the opposite conversion takes place.
 */
enum WGPUTextureFormat {
  /**
   * Red channel only. 8 bit integer per channel. [0, 255] converted to/from float [0, 1] in shader.
   */
  WGPUTextureFormat_R8Unorm = 0,
  /**
   * Red channel only. 8 bit integer per channel. [-127, 127] converted to/from float [-1, 1] in shader.
   */
  WGPUTextureFormat_R8Snorm = 1,
  /**
   * Red channel only. 8 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_R8Uint = 2,
  /**
   * Red channel only. 8 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_R8Sint = 3,
  /**
   * Red channel only. 16 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_R16Uint = 4,
  /**
   * Red channel only. 16 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_R16Sint = 5,
  /**
   * Red channel only. 16 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_R16Float = 6,
  /**
   * Red and green channels. 8 bit integer per channel. [0, 255] converted to/from float [0, 1] in shader.
   */
  WGPUTextureFormat_Rg8Unorm = 7,
  /**
   * Red and green channels. 8 bit integer per channel. [-127, 127] converted to/from float [-1, 1] in shader.
   */
  WGPUTextureFormat_Rg8Snorm = 8,
  /**
   * Red and green channels. 8 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rg8Uint = 9,
  /**
   * Red and green channel s. 8 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rg8Sint = 10,
  /**
   * Red channel only. 32 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_R32Uint = 11,
  /**
   * Red channel only. 32 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_R32Sint = 12,
  /**
   * Red channel only. 32 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_R32Float = 13,
  /**
   * Red and green channels. 16 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rg16Uint = 14,
  /**
   * Red and green channels. 16 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rg16Sint = 15,
  /**
   * Red and green channels. 16 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_Rg16Float = 16,
  /**
   * Red, green, blue, and alpha channels. 8 bit integer per channel. [0, 255] converted to/from float [0, 1] in shader.
   */
  WGPUTextureFormat_Rgba8Unorm = 17,
  /**
   * Red, green, blue, and alpha channels. 8 bit integer per channel. Srgb-color [0, 255] converted to/from linear-color float [0, 1] in shader.
   */
  WGPUTextureFormat_Rgba8UnormSrgb = 18,
  /**
   * Red, green, blue, and alpha channels. 8 bit integer per channel. [-127, 127] converted to/from float [-1, 1] in shader.
   */
  WGPUTextureFormat_Rgba8Snorm = 19,
  /**
   * Red, green, blue, and alpha channels. 8 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rgba8Uint = 20,
  /**
   * Red, green, blue, and alpha channels. 8 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rgba8Sint = 21,
  /**
   * Blue, green, red, and alpha channels. 8 bit integer per channel. [0, 255] converted to/from float [0, 1] in shader.
   */
  WGPUTextureFormat_Bgra8Unorm = 22,
  /**
   * Blue, green, red, and alpha channels. 8 bit integer per channel. Srgb-color [0, 255] converted to/from linear-color float [0, 1] in shader.
   */
  WGPUTextureFormat_Bgra8UnormSrgb = 23,
  /**
   * Red, green, blue, and alpha channels. 10 bit integer for RGB channels, 2 bit integer for alpha channel. [0, 1023] ([0, 3] for alpha) converted to/from float [0, 1] in shader.
   */
  WGPUTextureFormat_Rgb10a2Unorm = 24,
  /**
   * Red, green, and blue channels. 11 bit float with no sign bit for RG channels. 10 bit float with no sign bit for blue channel. Float in shader.
   */
  WGPUTextureFormat_Rg11b10Float = 25,
  /**
   * Red and green channels. 32 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rg32Uint = 26,
  /**
   * Red and green channels. 32 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rg32Sint = 27,
  /**
   * Red and green channels. 32 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_Rg32Float = 28,
  /**
   * Red, green, blue, and alpha channels. 16 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rgba16Uint = 29,
  /**
   * Red, green, blue, and alpha channels. 16 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rgba16Sint = 30,
  /**
   * Red, green, blue, and alpha channels. 16 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_Rgba16Float = 31,
  /**
   * Red, green, blue, and alpha channels. 32 bit integer per channel. Unsigned in shader.
   */
  WGPUTextureFormat_Rgba32Uint = 32,
  /**
   * Red, green, blue, and alpha channels. 32 bit integer per channel. Signed in shader.
   */
  WGPUTextureFormat_Rgba32Sint = 33,
  /**
   * Red, green, blue, and alpha channels. 32 bit float per channel. Float in shader.
   */
  WGPUTextureFormat_Rgba32Float = 34,
  /**
   * Special depth format with 32 bit floating point depth.
   */
  WGPUTextureFormat_Depth32Float = 35,
  /**
   * Special depth format with at least 24 bit integer depth.
   */
  WGPUTextureFormat_Depth24Plus = 36,
  /**
   * Special depth/stencil format with at least 24 bit integer depth and 8 bits integer stencil.
   */
  WGPUTextureFormat_Depth24PlusStencil8 = 37,
  /**
   * 4x4 block compressed texture. 8 bytes per block (4 bit/px). 4 color + alpha pallet. 5 bit R + 6 bit G + 5 bit B + 1 bit alpha.
   * [0, 64] ([0, 1] for alpha) converted to/from float [0, 1] in shader.
   *
   * Also known as DXT1.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc1RgbaUnorm = 38,
  /**
   * 4x4 block compressed texture. 8 bytes per block (4 bit/px). 4 color + alpha pallet. 5 bit R + 6 bit G + 5 bit B + 1 bit alpha.
   * Srgb-color [0, 64] ([0, 16] for alpha) converted to/from linear-color float [0, 1] in shader.
   *
   * Also known as DXT1.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc1RgbaUnormSrgb = 39,
  /**
   * 4x4 block compressed texture. 16 bytes per block (8 bit/px). 4 color pallet. 5 bit R + 6 bit G + 5 bit B + 4 bit alpha.
   * [0, 64] ([0, 16] for alpha) converted to/from float [0, 1] in shader.
   *
   * Also known as DXT3.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc2RgbaUnorm = 40,
  /**
   * 4x4 block compressed texture. 16 bytes per block (8 bit/px). 4 color pallet. 5 bit R + 6 bit G + 5 bit B + 4 bit alpha.
   * Srgb-color [0, 64] ([0, 256] for alpha) converted to/from linear-color float [0, 1] in shader.
   *
   * Also known as DXT3.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc2RgbaUnormSrgb = 41,
  /**
   * 4x4 block compressed texture. 16 bytes per block (8 bit/px). 4 color pallet + 8 alpha pallet. 5 bit R + 6 bit G + 5 bit B + 8 bit alpha.
   * [0, 64] ([0, 256] for alpha) converted to/from float [0, 1] in shader.
   *
   * Also known as DXT5.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc3RgbaUnorm = 42,
  /**
   * 4x4 block compressed texture. 16 bytes per block (8 bit/px). 4 color pallet + 8 alpha pallet. 5 bit R + 6 bit G + 5 bit B + 8 bit alpha.
   * Srgb-color [0, 64] ([0, 256] for alpha) converted to/from linear-color float [0, 1] in shader.
   *
   * Also known as DXT5.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc3RgbaUnormSrgb = 43,
  /**
   * 4x4 block compressed texture. 8 bytes per block (4 bit/px). 8 color pallet. 8 bit R.
   * [0, 256] converted to/from float [0, 1] in shader.
   *
   * Also known as RGTC1.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc4RUnorm = 44,
  /**
   * 4x4 block compressed texture. 8 bytes per block (4 bit/px). 8 color pallet. 8 bit R.
   * [-127, 127] converted to/from float [-1, 1] in shader.
   *
   * Also known as RGTC1.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc4RSnorm = 45,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). 8 color red pallet + 8 color green pallet. 8 bit RG.
   * [0, 256] converted to/from float [0, 1] in shader.
   *
   * Also known as RGTC2.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc5RgUnorm = 46,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). 8 color red pallet + 8 color green pallet. 8 bit RG.
   * [-127, 127] converted to/from float [-1, 1] in shader.
   *
   * Also known as RGTC2.
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc5RgSnorm = 47,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). Variable sized pallet. 16 bit unsigned float RGB. Float in shader.
   *
   * Also known as BPTC (float).
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc6hRgbUfloat = 48,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). Variable sized pallet. 16 bit signed float RGB. Float in shader.
   *
   * Also known as BPTC (float).
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc6hRgbSfloat = 49,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). Variable sized pallet. 8 bit integer RGBA.
   * [0, 256] converted to/from float [0, 1] in shader.
   *
   * Also known as BPTC (unorm).
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc7RgbaUnorm = 50,
  /**
   * 4x4 block compressed texture. 16 bytes per block (16 bit/px). Variable sized pallet. 8 bit integer RGBA.
   * Srgb-color [0, 255] converted to/from linear-color float [0, 1] in shader.
   *
   * Also known as BPTC (unorm).
   *
   * [`Features::TEXTURE_COMPRESSION_BC`] must be enabled to use this texture format.
   */
  WGPUTextureFormat_Bc7RgbaUnormSrgb = 51,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureFormat_Sentinel,
};

/**
 * Dimensions of a particular texture view.
 */
enum WGPUTextureViewDimension {
  /**
   * A one dimensional texture. `texture1D` in glsl shaders.
   */
  WGPUTextureViewDimension_D1,
  /**
   * A two dimensional texture. `texture2D` in glsl shaders.
   */
  WGPUTextureViewDimension_D2,
  /**
   * A two dimensional array texture. `texture2DArray` in glsl shaders.
   */
  WGPUTextureViewDimension_D2Array,
  /**
   * A cubemap texture. `textureCube` in glsl shaders.
   */
  WGPUTextureViewDimension_Cube,
  /**
   * A cubemap array texture. `textureCubeArray` in glsl shaders.
   */
  WGPUTextureViewDimension_CubeArray,
  /**
   * A three dimensional texture. `texture3D` in glsl shaders.
   */
  WGPUTextureViewDimension_D3,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureViewDimension_Sentinel,
};

/**
 * Vertex Format for a Vertex Attribute (input).
 */
enum WGPUVertexFormat {
  /**
   * Two unsigned bytes (u8). `uvec2` in shaders.
   */
  WGPUVertexFormat_Uchar2 = 0,
  /**
   * Four unsigned bytes (u8). `uvec4` in shaders.
   */
  WGPUVertexFormat_Uchar4 = 1,
  /**
   * Two signed bytes (i8). `ivec2` in shaders.
   */
  WGPUVertexFormat_Char2 = 2,
  /**
   * Four signed bytes (i8). `ivec4` in shaders.
   */
  WGPUVertexFormat_Char4 = 3,
  /**
   * Two unsigned bytes (u8). [0, 255] converted to float [0, 1] `vec2` in shaders.
   */
  WGPUVertexFormat_Uchar2Norm = 4,
  /**
   * Four unsigned bytes (u8). [0, 255] converted to float [0, 1] `vec4` in shaders.
   */
  WGPUVertexFormat_Uchar4Norm = 5,
  /**
   * Two signed bytes (i8). [-127, 127] converted to float [-1, 1] `vec2` in shaders.
   */
  WGPUVertexFormat_Char2Norm = 6,
  /**
   * Four signed bytes (i8). [-127, 127] converted to float [-1, 1] `vec4` in shaders.
   */
  WGPUVertexFormat_Char4Norm = 7,
  /**
   * Two unsigned shorts (u16). `uvec2` in shaders.
   */
  WGPUVertexFormat_Ushort2 = 8,
  /**
   * Four unsigned shorts (u16). `uvec4` in shaders.
   */
  WGPUVertexFormat_Ushort4 = 9,
  /**
   * Two signed shorts (i16). `ivec2` in shaders.
   */
  WGPUVertexFormat_Short2 = 10,
  /**
   * Four signed shorts (i16). `ivec4` in shaders.
   */
  WGPUVertexFormat_Short4 = 11,
  /**
   * Two unsigned shorts (u16). [0, 65535] converted to float [0, 1] `vec2` in shaders.
   */
  WGPUVertexFormat_Ushort2Norm = 12,
  /**
   * Four unsigned shorts (u16). [0, 65535] converted to float [0, 1] `vec4` in shaders.
   */
  WGPUVertexFormat_Ushort4Norm = 13,
  /**
   * Two signed shorts (i16). [-32767, 32767] converted to float [-1, 1] `vec2` in shaders.
   */
  WGPUVertexFormat_Short2Norm = 14,
  /**
   * Four signed shorts (i16). [-32767, 32767] converted to float [-1, 1] `vec4` in shaders.
   */
  WGPUVertexFormat_Short4Norm = 15,
  /**
   * Two half-precision floats (no Rust equiv). `vec2` in shaders.
   */
  WGPUVertexFormat_Half2 = 16,
  /**
   * Four half-precision floats (no Rust equiv). `vec4` in shaders.
   */
  WGPUVertexFormat_Half4 = 17,
  /**
   * One single-precision float (f32). `float` in shaders.
   */
  WGPUVertexFormat_Float = 18,
  /**
   * Two single-precision floats (f32). `vec2` in shaders.
   */
  WGPUVertexFormat_Float2 = 19,
  /**
   * Three single-precision floats (f32). `vec3` in shaders.
   */
  WGPUVertexFormat_Float3 = 20,
  /**
   * Four single-precision floats (f32). `vec4` in shaders.
   */
  WGPUVertexFormat_Float4 = 21,
  /**
   * One unsigned int (u32). `uint` in shaders.
   */
  WGPUVertexFormat_Uint = 22,
  /**
   * Two unsigned ints (u32). `uvec2` in shaders.
   */
  WGPUVertexFormat_Uint2 = 23,
  /**
   * Three unsigned ints (u32). `uvec3` in shaders.
   */
  WGPUVertexFormat_Uint3 = 24,
  /**
   * Four unsigned ints (u32). `uvec4` in shaders.
   */
  WGPUVertexFormat_Uint4 = 25,
  /**
   * One signed int (i32). `int` in shaders.
   */
  WGPUVertexFormat_Int = 26,
  /**
   * Two signed ints (i32). `ivec2` in shaders.
   */
  WGPUVertexFormat_Int2 = 27,
  /**
   * Three signed ints (i32). `ivec3` in shaders.
   */
  WGPUVertexFormat_Int3 = 28,
  /**
   * Four signed ints (i32). `ivec4` in shaders.
   */
  WGPUVertexFormat_Int4 = 29,
  /**
   * Must be last for serialization purposes
   */
  WGPUVertexFormat_Sentinel,
};

/**
 * The internal enum mirrored from `BufferUsage`. The values don't have to match!
 */
struct WGPUBufferUse;

struct WGPUClient;

struct WGPUComputePass;

struct WGPUGlobal;

/**
 * Describes a pipeline layout.
 *
 * A `PipelineLayoutDescriptor` can be used to create a pipeline layout.
 */
struct WGPUPipelineLayoutDescriptor;

struct WGPURenderBundleEncoder;

struct WGPURenderPass;

/**
 * The internal enum mirrored from `TextureUsage`. The values don't have to match!
 */
struct WGPUTextureUse;

struct WGPUInfrastructure {
  struct WGPUClient *client;
  const uint8_t *error;
};

typedef WGPUNonZeroU64 WGPUId_Adapter_Dummy;

typedef WGPUId_Adapter_Dummy WGPUAdapterId;

typedef WGPUNonZeroU64 WGPUId_Device_Dummy;

typedef WGPUId_Device_Dummy WGPUDeviceId;

typedef WGPUNonZeroU64 WGPUId_Buffer_Dummy;

typedef WGPUId_Buffer_Dummy WGPUBufferId;

typedef const char *WGPURawString;

/**
 * Integral type used for buffer offsets.
 */
typedef uint64_t WGPUBufferAddress;

/**
 * Different ways that you can use a buffer.
 *
 * The usages determine what kind of memory the buffer is allocated from and what
 * actions the buffer can partake in.
 */
typedef uint32_t WGPUBufferUsage;
/**
 * Allow a buffer to be mapped for reading using [`Buffer::map_async`] + [`Buffer::get_mapped_range`].
 * This does not include creating a buffer with [`BufferDescriptor::mapped_at_creation`] set.
 *
 * If [`Features::MAPPABLE_PRIMARY_BUFFERS`] isn't enabled, the only other usage a buffer
 * may have is COPY_DST.
 */
#define WGPUBufferUsage_MAP_READ (uint32_t)1
/**
 * Allow a buffer to be mapped for writing using [`Buffer::map_async`] + [`Buffer::get_mapped_range_mut`].
 * This does not include creating a buffer with `mapped_at_creation` set.
 *
 * If [`Features::MAPPABLE_PRIMARY_BUFFERS`] feature isn't enabled, the only other usage a buffer
 * may have is COPY_SRC.
 */
#define WGPUBufferUsage_MAP_WRITE (uint32_t)2
/**
 * Allow a buffer to be the source buffer for a [`CommandEncoder::copy_buffer_to_buffer`] or [`CommandEncoder::copy_buffer_to_texture`]
 * operation.
 */
#define WGPUBufferUsage_COPY_SRC (uint32_t)4
/**
 * Allow a buffer to be the destination buffer for a [`CommandEncoder::copy_buffer_to_buffer`], [`CommandEncoder::copy_texture_to_buffer`],
 * or [`Queue::write_buffer`] operation.
 */
#define WGPUBufferUsage_COPY_DST (uint32_t)8
/**
 * Allow a buffer to be the index buffer in a draw operation.
 */
#define WGPUBufferUsage_INDEX (uint32_t)16
/**
 * Allow a buffer to be the vertex buffer in a draw operation.
 */
#define WGPUBufferUsage_VERTEX (uint32_t)32
/**
 * Allow a buffer to be a [`BindingType::UniformBuffer`] inside a bind group.
 */
#define WGPUBufferUsage_UNIFORM (uint32_t)64
/**
 * Allow a buffer to be a [`BindingType::StorageBuffer`] inside a bind group.
 */
#define WGPUBufferUsage_STORAGE (uint32_t)128
/**
 * Allow a buffer to be the indirect buffer in an indirect draw call.
 */
#define WGPUBufferUsage_INDIRECT (uint32_t)256

/**
 * Describes a [`Buffer`].
 */
struct WGPUBufferDescriptor {
  /**
   * Debug label of a buffer. This will show up in graphics debuggers for easy identification.
   */
  WGPURawString label;
  /**
   * Size of a buffer.
   */
  WGPUBufferAddress size;
  /**
   * Usages of a buffer. If the buffer is used in any way that isn't specified here, the operation
   * will panic.
   */
  WGPUBufferUsage usage;
  /**
   * Allows a buffer to be mapped immediately after they are made. It does not have to be [`BufferUsage::MAP_READ`] or
   * [`BufferUsage::MAP_WRITE`], all buffers are allowed to be mapped at creation.
   */
  bool mapped_at_creation;
};

typedef WGPUNonZeroU64 WGPUId_Texture_Dummy;

typedef WGPUId_Texture_Dummy WGPUTextureId;

/**
 * Extent of a texture related operation.
 */
struct WGPUExtent3d {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
};

/**
 * Different ways that you can use a texture.
 *
 * The usages determine what kind of memory the texture is allocated from and what
 * actions the texture can partake in.
 */
typedef uint32_t WGPUTextureUsage;
/**
 * Allows a texture to be the source in a [`CommandEncoder::copy_texture_to_buffer`] or
 * [`CommandEncoder::copy_texture_to_texture`] operation.
 */
#define WGPUTextureUsage_COPY_SRC (uint32_t)1
/**
 * Allows a texture to be the destination in a  [`CommandEncoder::copy_texture_to_buffer`],
 * [`CommandEncoder::copy_texture_to_texture`], or [`Queue::write_texture`] operation.
 */
#define WGPUTextureUsage_COPY_DST (uint32_t)2
/**
 * Allows a texture to be a [`BindingType::SampledTexture`] in a bind group.
 */
#define WGPUTextureUsage_SAMPLED (uint32_t)4
/**
 * Allows a texture to be a [`BindingType::StorageTexture`] in a bind group.
 */
#define WGPUTextureUsage_STORAGE (uint32_t)8
/**
 * Allows a texture to be a output attachment of a renderpass.
 */
#define WGPUTextureUsage_OUTPUT_ATTACHMENT (uint32_t)16

/**
 * Describes a [`Texture`].
 */
struct WGPUTextureDescriptor {
  /**
   * Debug label of the texture. This will show up in graphics debuggers for easy identification.
   */
  WGPURawString label;
  /**
   * Size of the texture. For a regular 1D/2D texture, the unused sizes will be 1. For 2DArray textures, Z is the
   * number of 2D textures in that array.
   */
  struct WGPUExtent3d size;
  /**
   * Mip count of texture. For a texture with no extra mips, this must be 1.
   */
  uint32_t mip_level_count;
  /**
   * Sample count of texture. If this is not 1, texture must have [`BindingType::SampledTexture::multisampled`] set to true.
   */
  uint32_t sample_count;
  /**
   * Dimensions of the texture.
   */
  enum WGPUTextureDimension dimension;
  /**
   * Format of the texture.
   */
  enum WGPUTextureFormat format;
  /**
   * Allowed usages of the texture. If used in other ways, the operation will panic.
   */
  WGPUTextureUsage usage;
};

typedef WGPUNonZeroU64 WGPUId_TextureView_Dummy;

typedef WGPUId_TextureView_Dummy WGPUTextureViewId;

struct WGPUTextureViewDescriptor {
  WGPURawString label;
  const enum WGPUTextureFormat *format;
  const enum WGPUTextureViewDimension *dimension;
  enum WGPUTextureAspect aspect;
  uint32_t base_mip_level;
  WGPUOption_NonZeroU32 level_count;
  uint32_t base_array_layer;
  WGPUOption_NonZeroU32 array_layer_count;
};

typedef WGPUNonZeroU64 WGPUId_Sampler_Dummy;

typedef WGPUId_Sampler_Dummy WGPUSamplerId;

struct WGPUSamplerDescriptor {
  WGPURawString label;
  enum WGPUAddressMode address_modes[3];
  enum WGPUFilterMode mag_filter;
  enum WGPUFilterMode min_filter;
  enum WGPUFilterMode mipmap_filter;
  float lod_min_clamp;
  float lod_max_clamp;
  const enum WGPUCompareFunction *compare;
  WGPUOption_NonZeroU8 anisotropy_clamp;
};

typedef WGPUNonZeroU64 WGPUId_CommandBuffer_Dummy;

typedef WGPUId_CommandBuffer_Dummy WGPUCommandBufferId;

typedef WGPUCommandBufferId WGPUCommandEncoderId;

/**
 * Describes a [`CommandEncoder`].
 */
struct WGPUCommandEncoderDescriptor {
  /**
   * Debug label for the command encoder. This will show up in graphics debuggers for easy identification.
   */
  WGPURawString label;
};

struct WGPUComputePassDescriptor {
  uint32_t todo;
};

/**
 * RGBA double precision color.
 *
 * This is not to be used as a generic color type, only for specific wgpu interfaces.
 */
struct WGPUColor {
  double r;
  double g;
  double b;
  double a;
};
#define WGPUColor_TRANSPARENT (WGPUColor){ .r = 0.0, .g = 0.0, .b = 0.0, .a = 0.0 }
#define WGPUColor_BLACK (WGPUColor){ .r = 0.0, .g = 0.0, .b = 0.0, .a = 1.0 }
#define WGPUColor_WHITE (WGPUColor){ .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 }
#define WGPUColor_RED (WGPUColor){ .r = 1.0, .g = 0.0, .b = 0.0, .a = 1.0 }
#define WGPUColor_GREEN (WGPUColor){ .r = 0.0, .g = 1.0, .b = 0.0, .a = 1.0 }
#define WGPUColor_BLUE (WGPUColor){ .r = 0.0, .g = 0.0, .b = 1.0, .a = 1.0 }

/**
 * Describes an individual channel within a render pass, such as color, depth, or stencil.
 */
struct WGPUPassChannel_Color {
  /**
   * Operation to perform to the output attachment at the start of a renderpass. This must be clear if it
   * is the first renderpass rendering to a swap chain image.
   */
  enum WGPULoadOp load_op;
  /**
   * Operation to perform to the output attachment at the end of a renderpass.
   */
  enum WGPUStoreOp store_op;
  /**
   * If load_op is [`LoadOp::Clear`], the attachement will be cleared to this color.
   */
  struct WGPUColor clear_value;
  /**
   * If true, the relevant channel is not changed by a renderpass, and the corresponding attachment
   * can be used inside the pass by other read-only usages.
   */
  bool read_only;
};

/**
 * Describes a color attachment to a render pass.
 */
struct WGPUColorAttachmentDescriptor {
  /**
   * The view to use as an attachment.
   */
  WGPUTextureViewId attachment;
  /**
   * The view that will receive the resolved output if multisampling is used.
   */
  WGPUOption_TextureViewId resolve_target;
  /**
   * What operations will be performed on this color attachment.
   */
  struct WGPUPassChannel_Color channel;
};

/**
 * Describes an individual channel within a render pass, such as color, depth, or stencil.
 */
struct WGPUPassChannel_f32 {
  /**
   * Operation to perform to the output attachment at the start of a renderpass. This must be clear if it
   * is the first renderpass rendering to a swap chain image.
   */
  enum WGPULoadOp load_op;
  /**
   * Operation to perform to the output attachment at the end of a renderpass.
   */
  enum WGPUStoreOp store_op;
  /**
   * If load_op is [`LoadOp::Clear`], the attachement will be cleared to this color.
   */
  float clear_value;
  /**
   * If true, the relevant channel is not changed by a renderpass, and the corresponding attachment
   * can be used inside the pass by other read-only usages.
   */
  bool read_only;
};

/**
 * Describes an individual channel within a render pass, such as color, depth, or stencil.
 */
struct WGPUPassChannel_u32 {
  /**
   * Operation to perform to the output attachment at the start of a renderpass. This must be clear if it
   * is the first renderpass rendering to a swap chain image.
   */
  enum WGPULoadOp load_op;
  /**
   * Operation to perform to the output attachment at the end of a renderpass.
   */
  enum WGPUStoreOp store_op;
  /**
   * If load_op is [`LoadOp::Clear`], the attachement will be cleared to this color.
   */
  uint32_t clear_value;
  /**
   * If true, the relevant channel is not changed by a renderpass, and the corresponding attachment
   * can be used inside the pass by other read-only usages.
   */
  bool read_only;
};

/**
 * Describes a depth/stencil attachment to a render pass.
 */
struct WGPUDepthStencilAttachmentDescriptor {
  /**
   * The view to use as an attachment.
   */
  WGPUTextureViewId attachment;
  /**
   * What operations will be performed on the depth part of the attachment.
   */
  struct WGPUPassChannel_f32 depth;
  /**
   * What operations will be performed on the stencil part of the attachment.
   */
  struct WGPUPassChannel_u32 stencil;
};

struct WGPURenderPassDescriptor {
  const struct WGPUColorAttachmentDescriptor *color_attachments;
  uintptr_t color_attachments_length;
  const struct WGPUDepthStencilAttachmentDescriptor *depth_stencil_attachment;
};

typedef WGPUNonZeroU64 WGPUId_BindGroupLayout_Dummy;

typedef WGPUId_BindGroupLayout_Dummy WGPUBindGroupLayoutId;

typedef WGPUNonZeroU64 WGPUId_PipelineLayout_Dummy;

typedef WGPUId_PipelineLayout_Dummy WGPUPipelineLayoutId;

typedef WGPUNonZeroU64 WGPUId_BindGroup_Dummy;

typedef WGPUId_BindGroup_Dummy WGPUBindGroupId;

typedef WGPUNonZeroU64 WGPUId_ShaderModule_Dummy;

typedef WGPUId_ShaderModule_Dummy WGPUShaderModuleId;

struct WGPUShaderModuleDescriptor {
  const uint32_t *spirv_words;
  uintptr_t spirv_words_length;
  WGPURawString wgsl_chars;
};

typedef WGPUNonZeroU64 WGPUId_ComputePipeline_Dummy;

typedef WGPUId_ComputePipeline_Dummy WGPUComputePipelineId;

struct WGPUProgrammableStageDescriptor {
  WGPUShaderModuleId module;
  WGPURawString entry_point;
};

struct WGPUComputePipelineDescriptor {
  WGPURawString label;
  WGPUOption_PipelineLayoutId layout;
  struct WGPUProgrammableStageDescriptor compute_stage;
};

typedef WGPUNonZeroU64 WGPUId_RenderPipeline_Dummy;

typedef WGPUId_RenderPipeline_Dummy WGPURenderPipelineId;

/**
 * Describes the state of the rasterizer in a render pipeline.
 */
struct WGPURasterizationStateDescriptor {
  enum WGPUFrontFace front_face;
  enum WGPUCullMode cull_mode;
  /**
   * Controls the way each polygon is rasterized. Can be either `Fill` (default), `Line` or `Point`
   *
   * Setting this to something other than `Fill` requires `Features::NON_FILL_POLYGON_MODE` to be enabled.
   */
  enum WGPUPolygonMode polygon_mode;
  /**
   * If enabled polygon depth is clamped to 0-1 range instead of being clipped.
   *
   * Requires `Features::DEPTH_CLAMPING` enabled.
   */
  bool clamp_depth;
  int32_t depth_bias;
  float depth_bias_slope_scale;
  float depth_bias_clamp;
};

/**
 * Describes the blend state of a pipeline.
 *
 * Alpha blending is very complicated: see the OpenGL or Vulkan spec for more information.
 */
struct WGPUBlendDescriptor {
  enum WGPUBlendFactor src_factor;
  enum WGPUBlendFactor dst_factor;
  enum WGPUBlendOperation operation;
};

/**
 * Color write mask. Disabled color channels will not be written to.
 */
typedef uint32_t WGPUColorWrite;
/**
 * Enable red channel writes
 */
#define WGPUColorWrite_RED (uint32_t)1
/**
 * Enable green channel writes
 */
#define WGPUColorWrite_GREEN (uint32_t)2
/**
 * Enable blue channel writes
 */
#define WGPUColorWrite_BLUE (uint32_t)4
/**
 * Enable alpha channel writes
 */
#define WGPUColorWrite_ALPHA (uint32_t)8
/**
 * Enable red, green, and blue channel writes
 */
#define WGPUColorWrite_COLOR (uint32_t)7
/**
 * Enable writes to all channels.
 */
#define WGPUColorWrite_ALL (uint32_t)15

/**
 * Describes the color state of a render pipeline.
 */
struct WGPUColorStateDescriptor {
  /**
   * The [`TextureFormat`] of the image that this pipeline will render to. Must match the the format
   * of the corresponding color attachment in [`CommandEncoder::begin_render_pass`].
   */
  enum WGPUTextureFormat format;
  /**
   * The alpha blending that is used for this pipeline.
   */
  struct WGPUBlendDescriptor alpha_blend;
  /**
   * The color blending that is used for this pipeline.
   */
  struct WGPUBlendDescriptor color_blend;
  /**
   * Mask which enables/disables writes to different color/alpha channel.
   */
  WGPUColorWrite write_mask;
};

/**
 * Describes stencil state in a render pipeline.
 *
 * If you are not using stencil state, set this to [`StencilStateFaceDescriptor::IGNORE`].
 */
struct WGPUStencilStateFaceDescriptor {
  /**
   * Comparison function that determines if the fail_op or pass_op is used on the stencil buffer.
   */
  enum WGPUCompareFunction compare;
  /**
   * Operation that is preformed when stencil test fails.
   */
  enum WGPUStencilOperation fail_op;
  /**
   * Operation that is performed when depth test fails but stencil test succeeds.
   */
  enum WGPUStencilOperation depth_fail_op;
  /**
   * Operation that is performed when stencil test success.
   */
  enum WGPUStencilOperation pass_op;
};

struct WGPUStencilStateDescriptor {
  /**
   * Front face mode.
   */
  struct WGPUStencilStateFaceDescriptor front;
  /**
   * Back face mode.
   */
  struct WGPUStencilStateFaceDescriptor back;
  /**
   * Stencil values are AND'd with this mask when reading and writing from the stencil buffer. Only low 8 bits are used.
   */
  uint32_t read_mask;
  /**
   * Stencil values are AND'd with this mask when writing to the stencil buffer. Only low 8 bits are used.
   */
  uint32_t write_mask;
};

/**
 * Describes the depth/stencil state in a render pipeline.
 */
struct WGPUDepthStencilStateDescriptor {
  /**
   * Format of the depth/stencil buffer, must be special depth format. Must match the the format
   * of the depth/stencil attachment in [`CommandEncoder::begin_render_pass`].
   */
  enum WGPUTextureFormat format;
  /**
   * If disabled, depth will not be written to.
   */
  bool depth_write_enabled;
  /**
   * Comparison function used to compare depth values in the depth test.
   */
  enum WGPUCompareFunction depth_compare;
  struct WGPUStencilStateDescriptor stencil;
};

/**
 * Integral type used for binding locations in shaders.
 */
typedef uint32_t WGPUShaderLocation;

/**
 * Vertex inputs (attributes) to shaders.
 *
 * Arrays of these can be made with the [`vertex_attr_array`] macro. Vertex attributes are assumed to be tightly packed.
 */
struct WGPUVertexAttributeDescriptor {
  /**
   * Byte offset of the start of the input
   */
  WGPUBufferAddress offset;
  /**
   * Format of the input
   */
  enum WGPUVertexFormat format;
  /**
   * Location for this input. Must match the location in the shader.
   */
  WGPUShaderLocation shader_location;
};

struct WGPUVertexBufferDescriptor {
  WGPUBufferAddress stride;
  enum WGPUInputStepMode step_mode;
  const struct WGPUVertexAttributeDescriptor *attributes;
  uintptr_t attributes_length;
};

struct WGPUVertexStateDescriptor {
  enum WGPUIndexFormat index_format;
  const struct WGPUVertexBufferDescriptor *vertex_buffers;
  uintptr_t vertex_buffers_length;
};

struct WGPURenderPipelineDescriptor {
  WGPURawString label;
  WGPUOption_PipelineLayoutId layout;
  const struct WGPUProgrammableStageDescriptor *vertex_stage;
  const struct WGPUProgrammableStageDescriptor *fragment_stage;
  enum WGPUPrimitiveTopology primitive_topology;
  const struct WGPURasterizationStateDescriptor *rasterization_state;
  const struct WGPUColorStateDescriptor *color_states;
  uintptr_t color_states_length;
  const struct WGPUDepthStencilStateDescriptor *depth_stencil_state;
  struct WGPUVertexStateDescriptor vertex_state;
  uint32_t sample_count;
  uint32_t sample_mask;
  bool alpha_to_coverage_enabled;
};

typedef void *WGPUFactoryParam;

typedef WGPUNonZeroU64 WGPUId_SwapChain_Dummy;

typedef WGPUId_SwapChain_Dummy WGPUSwapChainId;

typedef WGPUNonZeroU64 WGPUId_RenderBundle;

typedef WGPUId_RenderBundle WGPURenderBundleId;

typedef WGPUNonZeroU64 WGPUId_Surface;

typedef WGPUId_Surface WGPUSurfaceId;

struct WGPUIdentityRecyclerFactory {
  WGPUFactoryParam param;
  void (*free_adapter)(WGPUAdapterId, WGPUFactoryParam);
  void (*free_device)(WGPUDeviceId, WGPUFactoryParam);
  void (*free_swap_chain)(WGPUSwapChainId, WGPUFactoryParam);
  void (*free_pipeline_layout)(WGPUPipelineLayoutId, WGPUFactoryParam);
  void (*free_shader_module)(WGPUShaderModuleId, WGPUFactoryParam);
  void (*free_bind_group_layout)(WGPUBindGroupLayoutId, WGPUFactoryParam);
  void (*free_bind_group)(WGPUBindGroupId, WGPUFactoryParam);
  void (*free_command_buffer)(WGPUCommandBufferId, WGPUFactoryParam);
  void (*free_render_bundle)(WGPURenderBundleId, WGPUFactoryParam);
  void (*free_render_pipeline)(WGPURenderPipelineId, WGPUFactoryParam);
  void (*free_compute_pipeline)(WGPUComputePipelineId, WGPUFactoryParam);
  void (*free_buffer)(WGPUBufferId, WGPUFactoryParam);
  void (*free_texture)(WGPUTextureId, WGPUFactoryParam);
  void (*free_texture_view)(WGPUTextureViewId, WGPUFactoryParam);
  void (*free_sampler)(WGPUSamplerId, WGPUFactoryParam);
  void (*free_surface)(WGPUSurfaceId, WGPUFactoryParam);
};

/**
 * Options for requesting adapter.
 */
struct WGPURequestAdapterOptions_SurfaceId {
  /**
   * Power preference for the adapter.
   */
  enum WGPUPowerPreference power_preference;
  /**
   * Surface that is required to be presentable with the requested adapter. This does not
   * create the surface, only guarantees that the adapter can present to said surface.
   */
  WGPUOption_SurfaceId compatible_surface;
};

typedef struct WGPURequestAdapterOptions_SurfaceId WGPURequestAdapterOptions;

/**
 * Features that are not guaranteed to be supported.
 *
 * These are either part of the webgpu standard, or are extension features supported by
 * wgpu when targeting native.
 *
 * If you want to use a feature, you need to first verify that the adapter supports
 * the feature. If the adapter does not support the feature, requesting a device with it enabled
 * will panic.
 */
typedef uint64_t WGPUFeatures;
/**
 * By default, polygon depth is clipped to 0-1 range. Anything outside of that range
 * is rejected, and respective fragments are not touched.
 *
 * With this extension, we can force clamping of the polygon depth to 0-1. That allows
 * shadow map occluders to be rendered into a tighter depth range.
 *
 * Supported platforms:
 * - desktops
 * - some mobile chips
 *
 * This is a web and native feature.
 */
#define WGPUFeatures_DEPTH_CLAMPING (uint64_t)1
/**
 * Enables BCn family of compressed textures. All BCn textures use 4x4 pixel blocks
 * with 8 or 16 bytes per block.
 *
 * Compressed textures sacrifice some quality in exchange for signifigantly reduced
 * bandwidth usage.
 *
 * Supported Platforms:
 * - desktops
 *
 * This is a web and native feature.
 */
#define WGPUFeatures_TEXTURE_COMPRESSION_BC (uint64_t)2
/**
 * Webgpu only allows the MAP_READ and MAP_WRITE buffer usage to be matched with
 * COPY_DST and COPY_SRC respectively. This removes this requirement.
 *
 * This is only beneficial on systems that share memory between CPU and GPU. If enabled
 * on a system that doesn't, this can severely hinder performance. Only use if you understand
 * the consequences.
 *
 * Supported platforms:
 * - All
 *
 * This is a native only feature.
 */
#define WGPUFeatures_MAPPABLE_PRIMARY_BUFFERS (uint64_t)65536
/**
 * Allows the user to create uniform arrays of sampled textures in shaders:
 *
 * eg. `uniform texture2D textures[10]`.
 *
 * This capability allows them to exist and to be indexed by compile time constant
 * values.
 *
 * Supported platforms:
 * - DX12
 * - Metal (with MSL 2.0+ on macOS 10.13+)
 * - Vulkan
 *
 * This is a native only feature.
 */
#define WGPUFeatures_SAMPLED_TEXTURE_BINDING_ARRAY (uint64_t)131072
/**
 * Allows shaders to index sampled texture arrays with dynamically uniform values:
 *
 * eg. `texture_array[uniform_value]`
 *
 * This capability means the hardware will also support SAMPLED_TEXTURE_BINDING_ARRAY.
 *
 * Supported platforms:
 * - DX12
 * - Metal (with MSL 2.0+ on macOS 10.13+)
 * - Vulkan's shaderSampledImageArrayDynamicIndexing feature
 *
 * This is a native only feature.
 */
#define WGPUFeatures_SAMPLED_TEXTURE_ARRAY_DYNAMIC_INDEXING (uint64_t)262144
/**
 * Allows shaders to index sampled texture arrays with dynamically non-uniform values:
 *
 * eg. `texture_array[vertex_data]`
 *
 * In order to use this capability, the corresponding GLSL extension must be enabled like so:
 *
 * `#extension GL_EXT_nonuniform_qualifier : require`
 *
 * and then used either as `nonuniformEXT` qualifier in variable declaration:
 *
 * eg. `layout(location = 0) nonuniformEXT flat in int vertex_data;`
 *
 * or as `nonuniformEXT` constructor:
 *
 * eg. `texture_array[nonuniformEXT(vertex_data)]`
 *
 * HLSL does not need any extension.
 *
 * This capability means the hardware will also support SAMPLED_TEXTURE_ARRAY_DYNAMIC_INDEXING
 * and SAMPLED_TEXTURE_BINDING_ARRAY.
 *
 * Supported platforms:
 * - DX12
 * - Metal (with MSL 2.0+ on macOS 10.13+)
 * - Vulkan 1.2+ (or VK_EXT_descriptor_indexing)'s shaderSampledImageArrayNonUniformIndexing feature)
 *
 * This is a native only feature.
 */
#define WGPUFeatures_SAMPLED_TEXTURE_ARRAY_NON_UNIFORM_INDEXING (uint64_t)524288
/**
 * Allows the user to create unsized uniform arrays of bindings:
 *
 * eg. `uniform texture2D textures[]`.
 *
 * If this capability is supported, SAMPLED_TEXTURE_ARRAY_NON_UNIFORM_INDEXING is very likely
 * to also be supported
 *
 * Supported platforms:
 * - DX12
 * - Vulkan 1.2+ (or VK_EXT_descriptor_indexing)'s runtimeDescriptorArray feature
 *
 * This is a native only feature.
 */
#define WGPUFeatures_UNSIZED_BINDING_ARRAY (uint64_t)1048576
/**
 * Allows the user to call [`RenderPass::multi_draw_indirect`] and [`RenderPass::multi_draw_indexed_indirect`].
 *
 * Allows multiple indirect calls to be dispatched from a single buffer.
 *
 * Supported platforms:
 * - DX12
 * - Metal
 * - Vulkan
 *
 * This is a native only feature.
 */
#define WGPUFeatures_MULTI_DRAW_INDIRECT (uint64_t)2097152
/**
 * Allows the user to call [`RenderPass::multi_draw_indirect_count`] and [`RenderPass::multi_draw_indexed_indirect_count`].
 *
 * This allows the use of a buffer containing the actual number of draw calls.
 *
 * Supported platforms:
 * - DX12
 * - Vulkan 1.2+ (or VK_KHR_draw_indirect_count)
 *
 * This is a native only feature.
 */
#define WGPUFeatures_MULTI_DRAW_INDIRECT_COUNT (uint64_t)4194304
/**
 * Allows the use of push constants: small, fast bits of memory that can be updated
 * inside a [`RenderPass`].
 *
 * Allows the user to call [`RenderPass::set_push_constants`], provide a non-empty array
 * to [`PipelineLayoutDescriptor`], and provide a non-zero limit to [`Limits::max_push_constant_size`].
 *
 * A block of push constants can be declared with `layout(push_constant) uniform Name {..}` in shaders.
 *
 * Supported platforms:
 * - DX12
 * - Vulkan
 * - Metal
 * - DX11 (emulated with uniforms)
 * - OpenGL (emulated with uniforms)
 *
 * This is a native only feature.
 */
#define WGPUFeatures_PUSH_CONSTANTS (uint64_t)8388608
/**
 * Allows the use of [`AddressMode::ClampToBorder`].
 *
 * Supported platforms:
 * - DX12
 * - Vulkan
 * - Metal (macOS 10.12+ only)
 * - DX11
 * - OpenGL
 *
 * This is a web and native feature.
 */
#define WGPUFeatures_ADDRESS_MODE_CLAMP_TO_BORDER (uint64_t)16777216
/**
 * Allows the user to set a non-fill polygon mode in [`RasterizationStateDescriptor::polygon_mode`]
 *
 * This allows drawing polygons/triangles as lines (wireframe) or points instead of filled
 *
 * Supported platforms:
 * - DX12
 * - Vulkan
 *
 * This is a native only feature.
 */
#define WGPUFeatures_NON_FILL_POLYGON_MODE (uint64_t)33554432
/**
 * Features which are part of the upstream WebGPU standard.
 */
#define WGPUFeatures_ALL_WEBGPU (uint64_t)65535
/**
 * Features that are only available when targeting native (not web).
 */
#define WGPUFeatures_ALL_NATIVE (uint64_t)18446744073709486080ULL

/**
 * Represents the sets of limits an adapter/device supports.
 *
 * Limits "better" than the default must be supported by the adapter and requested when requesting
 * a device. If limits "better" than the adapter supports are requested, requesting a device will panic.
 * Once a device is requested, you may only use resources up to the limits requested _even_ if the
 * adapter supports "better" limits.
 *
 * Requesting limits that are "better" than you need may cause performance to decrease because the
 * implementation needs to support more than is needed. You should ideally only request exactly what
 * you need.
 *
 * See also: https://gpuweb.github.io/gpuweb/#dictdef-gpulimits
 */
struct WGPULimits {
  /**
   * Amount of bind groups that can be attached to a pipeline at the same time. Defaults to 4. Higher is "better".
   */
  uint32_t max_bind_groups;
  /**
   * Amount of uniform buffer bindings that can be dynamic in a single pipeline. Defaults to 8. Higher is "better".
   */
  uint32_t max_dynamic_uniform_buffers_per_pipeline_layout;
  /**
   * Amount of storage buffer bindings that can be dynamic in a single pipeline. Defaults to 4. Higher is "better".
   */
  uint32_t max_dynamic_storage_buffers_per_pipeline_layout;
  /**
   * Amount of sampled textures visible in a single shader stage. Defaults to 16. Higher is "better".
   */
  uint32_t max_sampled_textures_per_shader_stage;
  /**
   * Amount of samplers visible in a single shader stage. Defaults to 16. Higher is "better".
   */
  uint32_t max_samplers_per_shader_stage;
  /**
   * Amount of storage buffers visible in a single shader stage. Defaults to 4. Higher is "better".
   */
  uint32_t max_storage_buffers_per_shader_stage;
  /**
   * Amount of storage textures visible in a single shader stage. Defaults to 4. Higher is "better".
   */
  uint32_t max_storage_textures_per_shader_stage;
  /**
   * Amount of uniform buffers visible in a single shader stage. Defaults to 12. Higher is "better".
   */
  uint32_t max_uniform_buffers_per_shader_stage;
  /**
   * Maximum size in bytes of a binding to a uniform buffer. Defaults to 16384. Higher is "better".
   */
  uint32_t max_uniform_buffer_binding_size;
  /**
   * Amount of storage available for push constants in bytes. Defaults to 0. Higher is "better".
   * Requesting more than 0 during device creation requires [`Features::PUSH_CONSTANTS`] to be enabled.
   *
   * Expect the size to be:
   * - Vulkan: 128-256 bytes
   * - DX12: 256 bytes
   * - Metal: 4096 bytes
   * - DX11 & OpenGL don't natively support push constants, and are emulated with uniforms,
   *   so this number is less useful.
   */
  uint32_t max_push_constant_size;
};

/**
 * Describes a [`Device`].
 */
struct WGPUDeviceDescriptor {
  /**
   * Features that the device should support. If any feature is not supported by
   * the adapter, creating a device will panic.
   */
  WGPUFeatures features;
  /**
   * Limits that the device should support. If any limit is "better" than the limit exposed by
   * the adapter, creating a device will panic.
   */
  struct WGPULimits limits;
  /**
   * Switch shader validation on/off. This is a temporary field
   * that will be removed once our validation logic is complete.
   */
  bool shader_validation;
};

typedef void (*WGPUBufferMapCallback)(enum WGPUBufferMapAsyncStatus status, uint8_t *userdata);

struct WGPUBufferMapOperation {
  enum WGPUHostMap host;
  WGPUBufferMapCallback callback;
  uint8_t *user_data;
};

/**
 * Describes a [`CommandBuffer`].
 */
struct WGPUCommandBufferDescriptor {
  WGPURawString label;
};

/**
 * Origin of a copy to/from a texture.
 */
struct WGPUOrigin3d {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};
#define WGPUOrigin3d_ZERO (WGPUOrigin3d){ .x = 0, .y = 0, .z = 0 }

/**
 * View of a texture which can be used to copy to/from a buffer/texture.
 */
struct WGPUTextureCopyView_TextureId {
  /**
   * The texture to be copied to/from.
   */
  WGPUTextureId texture;
  /**
   * The target mip level of the texture.
   */
  uint32_t mip_level;
  /**
   * The base texel of the texture in the selected `mip_level`.
   */
  struct WGPUOrigin3d origin;
};

typedef struct WGPUTextureCopyView_TextureId WGPUTextureCopyView;

/**
 * Layout of a texture in a buffer's memory.
 */
struct WGPUTextureDataLayout {
  /**
   * Offset into the buffer that is the start of the texture. Must be a multiple of texture block size.
   * For non-compressed textures, this is 1.
   */
  WGPUBufferAddress offset;
  /**
   * Bytes per "row" of the image. This represents one row of pixels in the x direction. Compressed
   * textures include multiple rows of pixels in each "row". May be 0 for 1D texture copies.
   *
   * Must be a multiple of 256 for [`CommandEncoder::copy_buffer_to_texture`] and [`CommandEncoder::copy_texture_to_buffer`].
   * [`Queue::write_texture`] does not have this requirement.
   *
   * Must be a multiple of the texture block size. For non-compressed textures, this is 1.
   */
  uint32_t bytes_per_row;
  /**
   * Rows that make up a single "image". Each "image" is one layer in the z direction of a 3D image. May be larger
   * than `copy_size.y`.
   *
   * May be 0 for 2D texture copies.
   */
  uint32_t rows_per_image;
};

/**
 * View of a buffer which can be used to copy to/from a texture.
 */
struct WGPUBufferCopyView_BufferId {
  /**
   * The buffer to be copied to/from.
   */
  WGPUBufferId buffer;
  /**
   * The layout of the texture data in this buffer.
   */
  struct WGPUTextureDataLayout layout;
};

typedef struct WGPUBufferCopyView_BufferId WGPUBufferCopyView;

typedef WGPUDeviceId WGPUQueueId;

/**
 * Describes the shader stages that a binding will be visible from.
 *
 * These can be combined so something that is visible from both vertex and fragment shaders can be defined as:
 *
 * `ShaderStage::VERTEX | ShaderStage::FRAGMENT`
 */
typedef uint32_t WGPUShaderStage;
/**
 * Binding is not visible from any shader stage.
 */
#define WGPUShaderStage_NONE (uint32_t)0
/**
 * Binding is visible from the vertex shader of a render pipeline.
 */
#define WGPUShaderStage_VERTEX (uint32_t)1
/**
 * Binding is visible from the fragment shader of a render pipeline.
 */
#define WGPUShaderStage_FRAGMENT (uint32_t)2
/**
 * Binding is visible from the compute shader of a compute pipeline.
 */
#define WGPUShaderStage_COMPUTE (uint32_t)4

typedef uint32_t WGPURawEnumOption_TextureViewDimension;

typedef uint32_t WGPURawEnumOption_TextureComponentType;

typedef uint32_t WGPURawEnumOption_TextureFormat;

struct WGPUBindGroupLayoutEntry {
  uint32_t binding;
  WGPUShaderStage visibility;
  enum WGPURawBindingType ty;
  bool has_dynamic_offset;
  WGPUOption_BufferSize min_binding_size;
  WGPURawEnumOption_TextureViewDimension view_dimension;
  WGPURawEnumOption_TextureComponentType texture_component_type;
  bool multisampled;
  WGPURawEnumOption_TextureFormat storage_texture_format;
};

struct WGPUBindGroupLayoutDescriptor {
  WGPURawString label;
  const struct WGPUBindGroupLayoutEntry *entries;
  uintptr_t entries_length;
};

struct WGPUBindGroupEntry {
  uint32_t binding;
  WGPUOption_BufferId buffer;
  WGPUBufferAddress offset;
  WGPUOption_BufferSize size;
  WGPUOption_SamplerId sampler;
  WGPUOption_TextureViewId texture_view;
};

struct WGPUBindGroupDescriptor {
  WGPURawString label;
  WGPUBindGroupLayoutId layout;
  const struct WGPUBindGroupEntry *entries;
  uintptr_t entries_length;
};

/**
 * Integral type used for dynamic bind group offsets.
 */
typedef uint32_t WGPUDynamicOffset;































/**
 * Bound uniform/storage buffer offsets must be aligned to this number.
 */
#define WGPUBIND_BUFFER_ALIGNMENT 256

/**
 * Buffer to buffer copy offsets and sizes must be aligned to this number.
 */
#define WGPUCOPY_BUFFER_ALIGNMENT 4

/**
 * Vertex buffer strides have to be aligned to this number.
 */
#define WGPUVERTEX_STRIDE_ALIGNMENT 4

WGPU_INLINE
struct WGPUInfrastructure wgpu_client_new(void)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe because improper use may lead to memory
 * problems. For example, a double-free may occur if the function is called
 * twice on the same raw pointer.
 */
WGPU_INLINE
void wgpu_client_delete(struct WGPUClient *aClient)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `id_length` elements.
 */
WGPU_INLINE
uintptr_t wgpu_client_make_adapter_ids(const struct WGPUClient *aClient,
                                       WGPUAdapterId *aIds,
                                       uintptr_t aIdLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_adapter_id(const struct WGPUClient *aClient,
                                 WGPUAdapterId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUDeviceId wgpu_client_make_device_id(const struct WGPUClient *aClient,
                                        WGPUAdapterId aAdapterId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_device_id(const struct WGPUClient *aClient,
                                WGPUDeviceId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUBufferId wgpu_client_make_buffer_id(const struct WGPUClient *aClient,
                                        WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUBufferId wgpu_client_create_buffer(const struct WGPUClient *aClient,
                                       WGPUDeviceId aDeviceId,
                                       const struct WGPUBufferDescriptor *aDesc,
                                       WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_buffer_id(const struct WGPUClient *aClient,
                                WGPUBufferId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUTextureId wgpu_client_create_texture(const struct WGPUClient *aClient,
                                         WGPUDeviceId aDeviceId,
                                         const struct WGPUTextureDescriptor *aDesc,
                                         WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_texture_id(const struct WGPUClient *aClient,
                                 WGPUTextureId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUTextureViewId wgpu_client_create_texture_view(const struct WGPUClient *aClient,
                                                  WGPUDeviceId aDeviceId,
                                                  const struct WGPUTextureViewDescriptor *aDesc,
                                                  WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_texture_view_id(const struct WGPUClient *aClient,
                                      WGPUTextureViewId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUSamplerId wgpu_client_create_sampler(const struct WGPUClient *aClient,
                                         WGPUDeviceId aDeviceId,
                                         const struct WGPUSamplerDescriptor *aDesc,
                                         WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_sampler_id(const struct WGPUClient *aClient,
                                 WGPUSamplerId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUCommandEncoderId wgpu_client_create_command_encoder(const struct WGPUClient *aClient,
                                                        WGPUDeviceId aDeviceId,
                                                        const struct WGPUCommandEncoderDescriptor *aDesc,
                                                        WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_encoder_id(const struct WGPUClient *aClient,
                                 WGPUCommandEncoderId aId)
WGPU_FUNC;

WGPU_INLINE
struct WGPUComputePass *wgpu_command_encoder_begin_compute_pass(WGPUCommandEncoderId aEncoderId,
                                                                const struct WGPUComputePassDescriptor *aDesc)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_finish(const struct WGPUComputePass *aPass,
                              WGPUByteBuf *aOutput)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_destroy(struct WGPUComputePass *aPass)
WGPU_FUNC;

WGPU_INLINE
struct WGPURenderPass *wgpu_command_encoder_begin_render_pass(WGPUCommandEncoderId aEncoderId,
                                                              const struct WGPURenderPassDescriptor *aDesc)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_finish(const struct WGPURenderPass *aPass,
                             WGPUByteBuf *aOutput)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_destroy(struct WGPURenderPass *aPass)
WGPU_FUNC;

WGPU_INLINE
WGPUBindGroupLayoutId wgpu_client_make_bind_group_layout_id(const struct WGPUClient *aClient,
                                                            WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_bind_group_layout_id(const struct WGPUClient *aClient,
                                           WGPUBindGroupLayoutId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUPipelineLayoutId wgpu_client_make_pipeline_layout_id(const struct WGPUClient *aClient,
                                                         WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_pipeline_layout_id(const struct WGPUClient *aClient,
                                         WGPUPipelineLayoutId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUBindGroupId wgpu_client_make_bind_group_id(const struct WGPUClient *aClient,
                                               WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_bind_group_id(const struct WGPUClient *aClient,
                                    WGPUBindGroupId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUShaderModuleId wgpu_client_create_shader_module(const struct WGPUClient *aClient,
                                                    WGPUDeviceId aDeviceId,
                                                    const struct WGPUShaderModuleDescriptor *aDesc,
                                                    WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_shader_module_id(const struct WGPUClient *aClient,
                                       WGPUShaderModuleId aId)
WGPU_FUNC;

WGPU_INLINE
WGPUComputePipelineId wgpu_client_create_compute_pipeline(const struct WGPUClient *aClient,
                                                          WGPUDeviceId aDeviceId,
                                                          const struct WGPUComputePipelineDescriptor *aDesc,
                                                          WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_compute_pipeline_id(const struct WGPUClient *aClient,
                                          WGPUComputePipelineId aId)
WGPU_FUNC;

WGPU_INLINE
WGPURenderPipelineId wgpu_client_create_render_pipeline(const struct WGPUClient *aClient,
                                                        WGPUDeviceId aDeviceId,
                                                        const struct WGPURenderPipelineDescriptor *aDesc,
                                                        WGPUByteBuf *aBb)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_render_pipeline_id(const struct WGPUClient *aClient,
                                         WGPURenderPipelineId aId)
WGPU_FUNC;

WGPU_INLINE
struct WGPUGlobal *wgpu_server_new(struct WGPUIdentityRecyclerFactory aFactory)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe because improper use may lead to memory
 * problems. For example, a double-free may occur if the function is called
 * twice on the same raw pointer.
 */
WGPU_INLINE
void wgpu_server_delete(struct WGPUGlobal *aGlobal)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_poll_all_devices(const struct WGPUGlobal *aGlobal,
                                  bool aForceWait)
WGPU_FUNC;

/**
 * Request an adapter according to the specified options.
 * Provide the list of IDs to pick from.
 *
 * Returns the index in this list, or -1 if unable to pick.
 *
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `id_length` elements.
 */
WGPU_INLINE
int8_t wgpu_server_instance_request_adapter(const struct WGPUGlobal *aGlobal,
                                            const WGPURequestAdapterOptions *aDesc,
                                            const WGPUAdapterId *aIds,
                                            uintptr_t aIdLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_adapter_request_device(const struct WGPUGlobal *aGlobal,
                                        WGPUAdapterId aSelfId,
                                        const struct WGPUDeviceDescriptor *aDesc,
                                        WGPUDeviceId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_adapter_drop(const struct WGPUGlobal *aGlobal,
                              WGPUAdapterId aAdapterId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_drop(const struct WGPUGlobal *aGlobal,
                             WGPUDeviceId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_buffer(const struct WGPUGlobal *aGlobal,
                                      WGPUDeviceId aSelfId,
                                      const struct WGPUBufferDescriptor *aDesc,
                                      WGPUBufferId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_map(const struct WGPUGlobal *aGlobal,
                            WGPUBufferId aBufferId,
                            WGPUBufferAddress aStart,
                            WGPUBufferAddress aSize,
                            struct WGPUBufferMapOperation aOperation)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `size` elements.
 */
WGPU_INLINE
uint8_t *wgpu_server_buffer_get_mapped_range(const struct WGPUGlobal *aGlobal,
                                             WGPUBufferId aBufferId,
                                             WGPUBufferAddress aStart,
                                             WGPUOption_BufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_unmap(const struct WGPUGlobal *aGlobal,
                              WGPUBufferId aBufferId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_drop(const struct WGPUGlobal *aGlobal,
                             WGPUBufferId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_encoder(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUCommandEncoderDescriptor *aDesc,
                                       WGPUCommandEncoderId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_finish(const struct WGPUGlobal *aGlobal,
                                WGPUCommandEncoderId aSelfId,
                                const struct WGPUCommandBufferDescriptor *aDesc)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_drop(const struct WGPUGlobal *aGlobal,
                              WGPUCommandEncoderId aSelfId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `byte_length` elements.
 */
WGPU_INLINE
void wgpu_server_command_buffer_drop(const struct WGPUGlobal *aGlobal,
                                     WGPUCommandBufferId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_buffer_to_buffer(const struct WGPUGlobal *aGlobal,
                                               WGPUCommandEncoderId aSelfId,
                                               WGPUBufferId aSourceId,
                                               WGPUBufferAddress aSourceOffset,
                                               WGPUBufferId aDestinationId,
                                               WGPUBufferAddress aDestinationOffset,
                                               WGPUBufferAddress aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_texture_to_buffer(const struct WGPUGlobal *aGlobal,
                                                WGPUCommandEncoderId aSelfId,
                                                const WGPUTextureCopyView *aSource,
                                                const WGPUBufferCopyView *aDestination,
                                                const struct WGPUExtent3d *aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_buffer_to_texture(const struct WGPUGlobal *aGlobal,
                                                WGPUCommandEncoderId aSelfId,
                                                const WGPUBufferCopyView *aSource,
                                                const WGPUTextureCopyView *aDestination,
                                                const struct WGPUExtent3d *aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_texture_to_texture(const struct WGPUGlobal *aGlobal,
                                                 WGPUCommandEncoderId aSelfId,
                                                 const WGPUTextureCopyView *aSource,
                                                 const WGPUTextureCopyView *aDestination,
                                                 const struct WGPUExtent3d *aSize)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointers are
 * valid for `color_attachments_length` and `command_length` elements,
 * respectively.
 */
WGPU_INLINE
void wgpu_server_encode_compute_pass(const struct WGPUGlobal *aGlobal,
                                     WGPUCommandEncoderId aSelfId,
                                     const WGPUByteBuf *aByteBuf)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointers are
 * valid for `color_attachments_length` and `command_length` elements,
 * respectively.
 */
WGPU_INLINE
void wgpu_server_encode_render_pass(const struct WGPUGlobal *aGlobal,
                                    WGPUCommandEncoderId aSelfId,
                                    const struct WGPURenderPass *aPass)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `command_buffer_id_length` elements.
 */
WGPU_INLINE
void wgpu_server_queue_submit(const struct WGPUGlobal *aGlobal,
                              WGPUQueueId aSelfId,
                              const WGPUCommandBufferId *aCommandBufferIds,
                              uintptr_t aCommandBufferIdLength)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `data_length` elements.
 */
WGPU_INLINE
void wgpu_server_queue_write_buffer(const struct WGPUGlobal *aGlobal,
                                    WGPUQueueId aSelfId,
                                    WGPUBufferId aBufferId,
                                    WGPUBufferAddress aBufferOffset,
                                    const uint8_t *aData,
                                    uintptr_t aDataLength)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `data_length` elements.
 */
WGPU_INLINE
void wgpu_server_queue_write_texture(const struct WGPUGlobal *aGlobal,
                                     WGPUQueueId aSelfId,
                                     const WGPUTextureCopyView *aDestination,
                                     const uint8_t *aData,
                                     uintptr_t aDataLength,
                                     const struct WGPUTextureDataLayout *aLayout,
                                     const struct WGPUExtent3d *aExtent)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `entries_length` elements.
 */
WGPU_INLINE
void wgpu_server_device_create_bind_group_layout(const struct WGPUGlobal *aGlobal,
                                                 WGPUDeviceId aSelfId,
                                                 const struct WGPUBindGroupLayoutDescriptor *aDesc,
                                                 WGPUBindGroupLayoutId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_bind_group_layout_drop(const struct WGPUGlobal *aGlobal,
                                        WGPUBindGroupLayoutId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_pipeline_layout(const struct WGPUGlobal *aGlobal,
                                               WGPUDeviceId aSelfId,
                                               const struct WGPUPipelineLayoutDescriptor *aDesc,
                                               WGPUPipelineLayoutId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_pipeline_layout_drop(const struct WGPUGlobal *aGlobal,
                                      WGPUPipelineLayoutId aSelfId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `entries_length` elements.
 */
WGPU_INLINE
void wgpu_server_device_create_bind_group(const struct WGPUGlobal *aGlobal,
                                          WGPUDeviceId aSelfId,
                                          const struct WGPUBindGroupDescriptor *aDesc,
                                          WGPUBindGroupId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_bind_group_drop(const struct WGPUGlobal *aGlobal,
                                 WGPUBindGroupId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_shader_module_drop(const struct WGPUGlobal *aGlobal,
                                    WGPUShaderModuleId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_compute_pipeline_drop(const struct WGPUGlobal *aGlobal,
                                       WGPUComputePipelineId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_render_pipeline_drop(const struct WGPUGlobal *aGlobal,
                                      WGPURenderPipelineId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_texture(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUTextureDescriptor *aDesc,
                                       WGPUTextureId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_create_view(const struct WGPUGlobal *aGlobal,
                                     WGPUTextureId aSelfId,
                                     const struct WGPUTextureViewDescriptor *aDesc,
                                     WGPUTextureViewId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_drop(const struct WGPUGlobal *aGlobal,
                              WGPUTextureId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_view_drop(const struct WGPUGlobal *aGlobal,
                                   WGPUTextureViewId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_sampler(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUSamplerDescriptor *aDesc,
                                       WGPUSamplerId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_sampler_drop(const struct WGPUGlobal *aGlobal,
                              WGPUSamplerId aSelfId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `offset_length` elements.
 */
WGPU_INLINE
void wgpu_render_bundle_set_bind_group(struct WGPURenderBundleEncoder *aBundle,
                                       uint32_t aIndex,
                                       WGPUBindGroupId aBindGroupId,
                                       const WGPUDynamicOffset *aOffsets,
                                       uintptr_t aOffsetLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_set_pipeline(struct WGPURenderBundleEncoder *aBundle,
                                     WGPURenderPipelineId aPipelineId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_set_index_buffer(struct WGPURenderBundleEncoder *aBundle,
                                         WGPUBufferId aBufferId,
                                         WGPUBufferAddress aOffset,
                                         WGPUOption_BufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_set_vertex_buffer(struct WGPURenderBundleEncoder *aBundle,
                                          uint32_t aSlot,
                                          WGPUBufferId aBufferId,
                                          WGPUBufferAddress aOffset,
                                          WGPUOption_BufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_set_push_constants(struct WGPURenderBundleEncoder *aPass,
                                           WGPUShaderStage aStages,
                                           uint32_t aOffset,
                                           uint32_t aSizeBytes,
                                           const uint8_t *aData)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_draw(struct WGPURenderBundleEncoder *aBundle,
                             uint32_t aVertexCount,
                             uint32_t aInstanceCount,
                             uint32_t aFirstVertex,
                             uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_draw_indexed(struct WGPURenderBundleEncoder *aBundle,
                                     uint32_t aIndexCount,
                                     uint32_t aInstanceCount,
                                     uint32_t aFirstIndex,
                                     int32_t aBaseVertex,
                                     uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_draw_indirect(struct WGPURenderBundleEncoder *aBundle,
                                      WGPUBufferId aBufferId,
                                      WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_bundle_indexed_indirect(struct WGPURenderBundleEncoder *aBundle,
                                              WGPUBufferId aBufferId,
                                              WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_push_debug_group(struct WGPURenderBundleEncoder *aBundle,
                                         WGPURawString aLabel)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_pop_debug_group(struct WGPURenderBundleEncoder *aBundle)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_bundle_insert_debug_marker(struct WGPURenderBundleEncoder *aBundle,
                                            WGPURawString aLabel)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `offset_length` elements.
 */
WGPU_INLINE
void wgpu_compute_pass_set_bind_group(struct WGPUComputePass *aPass,
                                      uint32_t aIndex,
                                      WGPUBindGroupId aBindGroupId,
                                      const WGPUDynamicOffset *aOffsets,
                                      uintptr_t aOffsetLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_set_pipeline(struct WGPUComputePass *aPass,
                                    WGPUComputePipelineId aPipelineId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_set_push_constant(struct WGPUComputePass *aPass,
                                         uint32_t aOffset,
                                         uint32_t aSizeBytes,
                                         const uint8_t *aData)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_dispatch(struct WGPUComputePass *aPass,
                                uint32_t aGroupsX,
                                uint32_t aGroupsY,
                                uint32_t aGroupsZ)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_dispatch_indirect(struct WGPUComputePass *aPass,
                                         WGPUBufferId aBufferId,
                                         WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_push_debug_group(struct WGPUComputePass *aPass,
                                        WGPURawString aLabel,
                                        uint32_t aColor)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_pop_debug_group(struct WGPUComputePass *aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_insert_debug_marker(struct WGPUComputePass *aPass,
                                           WGPURawString aLabel,
                                           uint32_t aColor)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `offset_length` elements.
 */
WGPU_INLINE
void wgpu_render_pass_set_bind_group(struct WGPURenderPass *aPass,
                                     uint32_t aIndex,
                                     WGPUBindGroupId aBindGroupId,
                                     const WGPUDynamicOffset *aOffsets,
                                     uintptr_t aOffsetLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_pipeline(struct WGPURenderPass *aPass,
                                   WGPURenderPipelineId aPipelineId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_index_buffer(struct WGPURenderPass *aPass,
                                       WGPUBufferId aBufferId,
                                       WGPUBufferAddress aOffset,
                                       WGPUOption_BufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_vertex_buffer(struct WGPURenderPass *aPass,
                                        uint32_t aSlot,
                                        WGPUBufferId aBufferId,
                                        WGPUBufferAddress aOffset,
                                        WGPUOption_BufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_blend_color(struct WGPURenderPass *aPass,
                                      const struct WGPUColor *aColor)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_stencil_reference(struct WGPURenderPass *aPass,
                                            uint32_t aValue)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_viewport(struct WGPURenderPass *aPass,
                                   float aX,
                                   float aY,
                                   float aW,
                                   float aH,
                                   float aDepthMin,
                                   float aDepthMax)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_scissor_rect(struct WGPURenderPass *aPass,
                                       uint32_t aX,
                                       uint32_t aY,
                                       uint32_t aW,
                                       uint32_t aH)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_push_constants(struct WGPURenderPass *aPass,
                                         WGPUShaderStage aStages,
                                         uint32_t aOffset,
                                         uint32_t aSizeBytes,
                                         const uint8_t *aData)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw(struct WGPURenderPass *aPass,
                           uint32_t aVertexCount,
                           uint32_t aInstanceCount,
                           uint32_t aFirstVertex,
                           uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indexed(struct WGPURenderPass *aPass,
                                   uint32_t aIndexCount,
                                   uint32_t aInstanceCount,
                                   uint32_t aFirstIndex,
                                   int32_t aBaseVertex,
                                   uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indirect(struct WGPURenderPass *aPass,
                                    WGPUBufferId aBufferId,
                                    WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indexed_indirect(struct WGPURenderPass *aPass,
                                            WGPUBufferId aBufferId,
                                            WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_multi_draw_indirect(struct WGPURenderPass *aPass,
                                          WGPUBufferId aBufferId,
                                          WGPUBufferAddress aOffset,
                                          uint32_t aCount)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_multi_draw_indexed_indirect(struct WGPURenderPass *aPass,
                                                  WGPUBufferId aBufferId,
                                                  WGPUBufferAddress aOffset,
                                                  uint32_t aCount)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_multi_draw_indirect_count(struct WGPURenderPass *aPass,
                                                WGPUBufferId aBufferId,
                                                WGPUBufferAddress aOffset,
                                                WGPUBufferId aCountBufferId,
                                                WGPUBufferAddress aCountBufferOffset,
                                                uint32_t aMaxCount)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_multi_draw_indexed_indirect_count(struct WGPURenderPass *aPass,
                                                        WGPUBufferId aBufferId,
                                                        WGPUBufferAddress aOffset,
                                                        WGPUBufferId aCountBufferId,
                                                        WGPUBufferAddress aCountBufferOffset,
                                                        uint32_t aMaxCount)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_push_debug_group(struct WGPURenderPass *aPass,
                                       WGPURawString aLabel,
                                       uint32_t aColor)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_pop_debug_group(struct WGPURenderPass *aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_insert_debug_marker(struct WGPURenderPass *aPass,
                                          WGPURawString aLabel,
                                          uint32_t aColor)
WGPU_FUNC;
