/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.14.4 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --lockfile Cargo.lock --crate wgpu_bindings -o dom/webgpu/ffi/wgpu_ffi_generated.h`
 */

typedef uint64_t WGPUNonZeroU64;
typedef uint64_t WGPUOption_AdapterId;
typedef uint64_t WGPUOption_SurfaceId;
typedef uint64_t WGPUOption_TextureViewId;
typedef char WGPUNonExhaustive[0];


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Bound uniform/storage buffer offsets must be aligned to this number.
 */
#define WGPUBIND_BUFFER_ALIGNMENT 256

/**
 * Buffer to buffer copy offsets and sizes must be aligned to this number
 */
#define WGPUCOPY_BUFFER_ALIGNMENT 4

/**
 * Buffer-Texture copies on command encoders have to have the `bytes_per_row`
 * aligned to this number.
 *
 * This doesn't apply to `Queue::write_texture`.
 */
#define WGPUCOPY_BYTES_PER_ROW_ALIGNMENT 256

#define WGPUDEFAULT_BIND_GROUPS 4

#define WGPUDESIRED_NUM_FRAMES 3

#define WGPUMAX_ANISOTROPY 16

#define WGPUMAX_COLOR_TARGETS 4

#define WGPUMAX_MIP_LEVELS 16

#define WGPUMAX_VERTEX_BUFFERS 16

enum WGPUAddressMode {
  WGPUAddressMode_ClampToEdge = 0,
  WGPUAddressMode_Repeat = 1,
  WGPUAddressMode_MirrorRepeat = 2,
  /**
   * Must be last for serialization purposes
   */
  WGPUAddressMode_Sentinel,
};

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

enum WGPUCompareFunction {
  WGPUCompareFunction_Undefined = 0,
  WGPUCompareFunction_Never = 1,
  WGPUCompareFunction_Less = 2,
  WGPUCompareFunction_Equal = 3,
  WGPUCompareFunction_LessEqual = 4,
  WGPUCompareFunction_Greater = 5,
  WGPUCompareFunction_NotEqual = 6,
  WGPUCompareFunction_GreaterEqual = 7,
  WGPUCompareFunction_Always = 8,
  /**
   * Must be last for serialization purposes
   */
  WGPUCompareFunction_Sentinel,
};

enum WGPUCullMode {
  WGPUCullMode_None = 0,
  WGPUCullMode_Front = 1,
  WGPUCullMode_Back = 2,
  /**
   * Must be last for serialization purposes
   */
  WGPUCullMode_Sentinel,
};

enum WGPUFilterMode {
  WGPUFilterMode_Nearest = 0,
  WGPUFilterMode_Linear = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUFilterMode_Sentinel,
};

enum WGPUFrontFace {
  WGPUFrontFace_Ccw = 0,
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

enum WGPUIndexFormat {
  WGPUIndexFormat_Uint16 = 0,
  WGPUIndexFormat_Uint32 = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUIndexFormat_Sentinel,
};

enum WGPUInputStepMode {
  WGPUInputStepMode_Vertex = 0,
  WGPUInputStepMode_Instance = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUInputStepMode_Sentinel,
};

enum WGPULoadOp {
  WGPULoadOp_Clear = 0,
  WGPULoadOp_Load = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPULoadOp_Sentinel,
};

enum WGPUPowerPreference {
  WGPUPowerPreference_Default = 0,
  WGPUPowerPreference_LowPower = 1,
  WGPUPowerPreference_HighPerformance = 2,
  /**
   * Must be last for serialization purposes
   */
  WGPUPowerPreference_Sentinel,
};

enum WGPUPrimitiveTopology {
  WGPUPrimitiveTopology_PointList = 0,
  WGPUPrimitiveTopology_LineList = 1,
  WGPUPrimitiveTopology_LineStrip = 2,
  WGPUPrimitiveTopology_TriangleList = 3,
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

enum WGPUStencilOperation {
  WGPUStencilOperation_Keep = 0,
  WGPUStencilOperation_Zero = 1,
  WGPUStencilOperation_Replace = 2,
  WGPUStencilOperation_Invert = 3,
  WGPUStencilOperation_IncrementClamp = 4,
  WGPUStencilOperation_DecrementClamp = 5,
  WGPUStencilOperation_IncrementWrap = 6,
  WGPUStencilOperation_DecrementWrap = 7,
  /**
   * Must be last for serialization purposes
   */
  WGPUStencilOperation_Sentinel,
};

enum WGPUStoreOp {
  WGPUStoreOp_Clear = 0,
  WGPUStoreOp_Store = 1,
  /**
   * Must be last for serialization purposes
   */
  WGPUStoreOp_Sentinel,
};

enum WGPUTextureAspect {
  WGPUTextureAspect_All,
  WGPUTextureAspect_StencilOnly,
  WGPUTextureAspect_DepthOnly,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureAspect_Sentinel,
};

enum WGPUTextureComponentType {
  WGPUTextureComponentType_Float,
  WGPUTextureComponentType_Sint,
  WGPUTextureComponentType_Uint,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureComponentType_Sentinel,
};

enum WGPUTextureDimension {
  WGPUTextureDimension_D1,
  WGPUTextureDimension_D2,
  WGPUTextureDimension_D3,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureDimension_Sentinel,
};

enum WGPUTextureFormat {
  WGPUTextureFormat_R8Unorm = 0,
  WGPUTextureFormat_R8Snorm = 1,
  WGPUTextureFormat_R8Uint = 2,
  WGPUTextureFormat_R8Sint = 3,
  WGPUTextureFormat_R16Uint = 4,
  WGPUTextureFormat_R16Sint = 5,
  WGPUTextureFormat_R16Float = 6,
  WGPUTextureFormat_Rg8Unorm = 7,
  WGPUTextureFormat_Rg8Snorm = 8,
  WGPUTextureFormat_Rg8Uint = 9,
  WGPUTextureFormat_Rg8Sint = 10,
  WGPUTextureFormat_R32Uint = 11,
  WGPUTextureFormat_R32Sint = 12,
  WGPUTextureFormat_R32Float = 13,
  WGPUTextureFormat_Rg16Uint = 14,
  WGPUTextureFormat_Rg16Sint = 15,
  WGPUTextureFormat_Rg16Float = 16,
  WGPUTextureFormat_Rgba8Unorm = 17,
  WGPUTextureFormat_Rgba8UnormSrgb = 18,
  WGPUTextureFormat_Rgba8Snorm = 19,
  WGPUTextureFormat_Rgba8Uint = 20,
  WGPUTextureFormat_Rgba8Sint = 21,
  WGPUTextureFormat_Bgra8Unorm = 22,
  WGPUTextureFormat_Bgra8UnormSrgb = 23,
  WGPUTextureFormat_Rgb10a2Unorm = 24,
  WGPUTextureFormat_Rg11b10Float = 25,
  WGPUTextureFormat_Rg32Uint = 26,
  WGPUTextureFormat_Rg32Sint = 27,
  WGPUTextureFormat_Rg32Float = 28,
  WGPUTextureFormat_Rgba16Uint = 29,
  WGPUTextureFormat_Rgba16Sint = 30,
  WGPUTextureFormat_Rgba16Float = 31,
  WGPUTextureFormat_Rgba32Uint = 32,
  WGPUTextureFormat_Rgba32Sint = 33,
  WGPUTextureFormat_Rgba32Float = 34,
  WGPUTextureFormat_Depth32Float = 35,
  WGPUTextureFormat_Depth24Plus = 36,
  WGPUTextureFormat_Depth24PlusStencil8 = 37,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureFormat_Sentinel,
};

enum WGPUTextureViewDimension {
  WGPUTextureViewDimension_D1,
  WGPUTextureViewDimension_D2,
  WGPUTextureViewDimension_D2Array,
  WGPUTextureViewDimension_Cube,
  WGPUTextureViewDimension_CubeArray,
  WGPUTextureViewDimension_D3,
  /**
   * Must be last for serialization purposes
   */
  WGPUTextureViewDimension_Sentinel,
};

enum WGPUVertexFormat {
  WGPUVertexFormat_Uchar2 = 0,
  WGPUVertexFormat_Uchar4 = 1,
  WGPUVertexFormat_Char2 = 2,
  WGPUVertexFormat_Char4 = 3,
  WGPUVertexFormat_Uchar2Norm = 4,
  WGPUVertexFormat_Uchar4Norm = 5,
  WGPUVertexFormat_Char2Norm = 6,
  WGPUVertexFormat_Char4Norm = 7,
  WGPUVertexFormat_Ushort2 = 8,
  WGPUVertexFormat_Ushort4 = 9,
  WGPUVertexFormat_Short2 = 10,
  WGPUVertexFormat_Short4 = 11,
  WGPUVertexFormat_Ushort2Norm = 12,
  WGPUVertexFormat_Ushort4Norm = 13,
  WGPUVertexFormat_Short2Norm = 14,
  WGPUVertexFormat_Short4Norm = 15,
  WGPUVertexFormat_Half2 = 16,
  WGPUVertexFormat_Half4 = 17,
  WGPUVertexFormat_Float = 18,
  WGPUVertexFormat_Float2 = 19,
  WGPUVertexFormat_Float3 = 20,
  WGPUVertexFormat_Float4 = 21,
  WGPUVertexFormat_Uint = 22,
  WGPUVertexFormat_Uint2 = 23,
  WGPUVertexFormat_Uint3 = 24,
  WGPUVertexFormat_Uint4 = 25,
  WGPUVertexFormat_Int = 26,
  WGPUVertexFormat_Int2 = 27,
  WGPUVertexFormat_Int3 = 28,
  WGPUVertexFormat_Int4 = 29,
  /**
   * Must be last for serialization purposes
   */
  WGPUVertexFormat_Sentinel,
};

struct WGPUBindGroupDescriptor;

/**
 * The internal enum mirrored from `BufferUsage`. The values don't have to match!
 */
struct WGPUBufferUse;

struct WGPUClient;

struct WGPUGlobal;

/**
 * The internal enum mirrored from `TextureUsage`. The values don't have to match!
 */
struct WGPUTextureUse;

typedef uint64_t WGPUBufferSize;

typedef WGPUNonZeroU64 WGPUId_Adapter_Dummy;

typedef WGPUId_Adapter_Dummy WGPUAdapterId;

typedef WGPUNonZeroU64 WGPUId_BindGroup_Dummy;

typedef WGPUId_BindGroup_Dummy WGPUBindGroupId;

typedef WGPUNonZeroU64 WGPUId_BindGroupLayout_Dummy;

typedef WGPUId_BindGroupLayout_Dummy WGPUBindGroupLayoutId;

typedef WGPUNonZeroU64 WGPUId_Buffer_Dummy;

typedef WGPUId_Buffer_Dummy WGPUBufferId;

typedef WGPUNonZeroU64 WGPUId_ComputePipeline_Dummy;

typedef WGPUId_ComputePipeline_Dummy WGPUComputePipelineId;

typedef WGPUNonZeroU64 WGPUId_Device_Dummy;

typedef WGPUId_Device_Dummy WGPUDeviceId;

typedef WGPUNonZeroU64 WGPUId_CommandBuffer_Dummy;

typedef WGPUId_CommandBuffer_Dummy WGPUCommandBufferId;

typedef WGPUCommandBufferId WGPUCommandEncoderId;

typedef WGPUNonZeroU64 WGPUId_PipelineLayout_Dummy;

typedef WGPUId_PipelineLayout_Dummy WGPUPipelineLayoutId;

typedef WGPUNonZeroU64 WGPUId_RenderPipeline_Dummy;

typedef WGPUId_RenderPipeline_Dummy WGPURenderPipelineId;

typedef WGPUNonZeroU64 WGPUId_Sampler_Dummy;

typedef WGPUId_Sampler_Dummy WGPUSamplerId;

typedef WGPUNonZeroU64 WGPUId_ShaderModule_Dummy;

typedef WGPUId_ShaderModule_Dummy WGPUShaderModuleId;

typedef WGPUNonZeroU64 WGPUId_Texture_Dummy;

typedef WGPUId_Texture_Dummy WGPUTextureId;

typedef WGPUNonZeroU64 WGPUId_TextureView_Dummy;

typedef WGPUId_TextureView_Dummy WGPUTextureViewId;

struct WGPUInfrastructure {
  struct WGPUClient *client;
  const uint8_t *error;
};

struct WGPURawPass {
  uint8_t *data;
  uint8_t *base;
  uintptr_t capacity;
  WGPUCommandEncoderId parent;
};

struct WGPUComputePassDescriptor {
  uint32_t todo;
};

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

struct WGPURenderPassColorAttachmentDescriptorBase_TextureViewId {
  WGPUTextureViewId attachment;
  WGPUOption_TextureViewId resolve_target;
  enum WGPULoadOp load_op;
  enum WGPUStoreOp store_op;
  struct WGPUColor clear_color;
};

typedef struct WGPURenderPassColorAttachmentDescriptorBase_TextureViewId WGPURenderPassColorAttachmentDescriptor;

struct WGPURenderPassDepthStencilAttachmentDescriptorBase_TextureViewId {
  WGPUTextureViewId attachment;
  enum WGPULoadOp depth_load_op;
  enum WGPUStoreOp depth_store_op;
  float clear_depth;
  bool depth_read_only;
  enum WGPULoadOp stencil_load_op;
  enum WGPUStoreOp stencil_store_op;
  uint32_t clear_stencil;
  bool stencil_read_only;
};

typedef struct WGPURenderPassDepthStencilAttachmentDescriptorBase_TextureViewId WGPURenderPassDepthStencilAttachmentDescriptor;

struct WGPURenderPassDescriptor {
  const WGPURenderPassColorAttachmentDescriptor *color_attachments;
  uintptr_t color_attachments_length;
  const WGPURenderPassDepthStencilAttachmentDescriptor *depth_stencil_attachment;
};

typedef uint64_t WGPUBufferAddress;

typedef const char *WGPURawString;

typedef uint32_t WGPUDynamicOffset;

typedef WGPUNonZeroU64 WGPUId_RenderBundle_Dummy;

typedef WGPUId_RenderBundle_Dummy WGPURenderBundleId;

typedef uint64_t WGPUExtensions;
/**
 * Allow anisotropic filtering in samplers.
 *
 * Supported platforms:
 * - OpenGL 4.6+ (or 1.2+ with widespread GL_EXT_texture_filter_anisotropic)
 * - DX11/12
 * - Metal
 * - Vulkan
 *
 * This is a native only extension. Support is planned to be added to webgpu,
 * but it is not yet implemented.
 *
 * https://github.com/gpuweb/gpuweb/issues/696
 */
#define WGPUExtensions_ANISOTROPIC_FILTERING (uint64_t)65536
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
 * This is a native only extension.
 */
#define WGPUExtensions_MAPPABLE_PRIMARY_BUFFERS (uint64_t)131072
/**
 * Allows the user to create uniform arrays of textures in shaders:
 *
 * eg. `uniform texture2D textures[10]`.
 *
 * This extension only allows them to exist and to be indexed by compile time constant
 * values.
 *
 * Supported platforms:
 * - DX12
 * - Metal (with MSL 2.0+ on macOS 10.13+)
 * - Vulkan
 *
 * This is a native only extension.
 */
#define WGPUExtensions_TEXTURE_BINDING_ARRAY (uint64_t)262144
/**
 * Extensions which are part of the upstream webgpu standard
 */
#define WGPUExtensions_ALL_WEBGPU (uint64_t)65535
/**
 * Extensions that require activating the unsafe extension flag
 */
#define WGPUExtensions_ALL_UNSAFE (uint64_t)18446462598732840960ULL
/**
 * Extensions that are only available when targeting native (not web)
 */
#define WGPUExtensions_ALL_NATIVE (uint64_t)18446744073709486080ULL

struct WGPULimits {
  uint32_t max_bind_groups;
  WGPUNonExhaustive _non_exhaustive;
};

struct WGPUDeviceDescriptor {
  WGPUExtensions extensions;
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

typedef uint32_t WGPUShaderStage;
#define WGPUShaderStage_NONE (uint32_t)0
#define WGPUShaderStage_VERTEX (uint32_t)1
#define WGPUShaderStage_FRAGMENT (uint32_t)2
#define WGPUShaderStage_COMPUTE (uint32_t)4

typedef uint32_t WGPURawEnumOption_TextureViewDimension;

typedef uint32_t WGPURawEnumOption_TextureComponentType;

typedef uint32_t WGPURawEnumOption_TextureFormat;

struct WGPUBindGroupLayoutEntry {
  uint32_t binding;
  WGPUShaderStage visibility;
  enum WGPURawBindingType ty;
  bool has_dynamic_offset;
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

typedef uint32_t WGPUBufferUsage;
#define WGPUBufferUsage_MAP_READ (uint32_t)1
#define WGPUBufferUsage_MAP_WRITE (uint32_t)2
#define WGPUBufferUsage_COPY_SRC (uint32_t)4
#define WGPUBufferUsage_COPY_DST (uint32_t)8
#define WGPUBufferUsage_INDEX (uint32_t)16
#define WGPUBufferUsage_VERTEX (uint32_t)32
#define WGPUBufferUsage_UNIFORM (uint32_t)64
#define WGPUBufferUsage_STORAGE (uint32_t)128
#define WGPUBufferUsage_INDIRECT (uint32_t)256

struct WGPUBufferDescriptor {
  WGPURawString label;
  WGPUBufferAddress size;
  WGPUBufferUsage usage;
  bool mapped_at_creation;
};

struct WGPUProgrammableStageDescriptor {
  WGPUShaderModuleId module;
  WGPURawString entry_point;
};

struct WGPUComputePipelineDescriptor {
  WGPUPipelineLayoutId layout;
  struct WGPUProgrammableStageDescriptor compute_stage;
};

struct WGPUCommandEncoderDescriptor {
  const char *label;
};

struct WGPUPipelineLayoutDescriptor {
  const WGPUBindGroupLayoutId *bind_group_layouts;
  uintptr_t bind_group_layouts_length;
};

struct WGPURasterizationStateDescriptor {
  enum WGPUFrontFace front_face;
  enum WGPUCullMode cull_mode;
  int32_t depth_bias;
  float depth_bias_slope_scale;
  float depth_bias_clamp;
};

struct WGPUBlendDescriptor {
  enum WGPUBlendFactor src_factor;
  enum WGPUBlendFactor dst_factor;
  enum WGPUBlendOperation operation;
};

typedef uint32_t WGPUColorWrite;
#define WGPUColorWrite_RED (uint32_t)1
#define WGPUColorWrite_GREEN (uint32_t)2
#define WGPUColorWrite_BLUE (uint32_t)4
#define WGPUColorWrite_ALPHA (uint32_t)8
#define WGPUColorWrite_COLOR (uint32_t)7
#define WGPUColorWrite_ALL (uint32_t)15

struct WGPUColorStateDescriptor {
  enum WGPUTextureFormat format;
  struct WGPUBlendDescriptor alpha_blend;
  struct WGPUBlendDescriptor color_blend;
  WGPUColorWrite write_mask;
};

struct WGPUStencilStateFaceDescriptor {
  enum WGPUCompareFunction compare;
  enum WGPUStencilOperation fail_op;
  enum WGPUStencilOperation depth_fail_op;
  enum WGPUStencilOperation pass_op;
};

struct WGPUDepthStencilStateDescriptor {
  enum WGPUTextureFormat format;
  bool depth_write_enabled;
  enum WGPUCompareFunction depth_compare;
  struct WGPUStencilStateFaceDescriptor stencil_front;
  struct WGPUStencilStateFaceDescriptor stencil_back;
  uint32_t stencil_read_mask;
  uint32_t stencil_write_mask;
};

typedef uint32_t WGPUShaderLocation;

struct WGPUVertexAttributeDescriptor {
  WGPUBufferAddress offset;
  enum WGPUVertexFormat format;
  WGPUShaderLocation shader_location;
};

struct WGPUVertexBufferLayoutDescriptor {
  WGPUBufferAddress array_stride;
  enum WGPUInputStepMode step_mode;
  const struct WGPUVertexAttributeDescriptor *attributes;
  uintptr_t attributes_length;
};

struct WGPUVertexStateDescriptor {
  enum WGPUIndexFormat index_format;
  const struct WGPUVertexBufferLayoutDescriptor *vertex_buffers;
  uintptr_t vertex_buffers_length;
};

struct WGPURenderPipelineDescriptor {
  WGPUPipelineLayoutId layout;
  struct WGPUProgrammableStageDescriptor vertex_stage;
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

struct WGPUSamplerDescriptor {
  WGPURawString label;
  enum WGPUAddressMode address_modes[3];
  enum WGPUFilterMode mag_filter;
  enum WGPUFilterMode min_filter;
  enum WGPUFilterMode mipmap_filter;
  float lod_min_clamp;
  float lod_max_clamp;
  const enum WGPUCompareFunction *compare;
  uint8_t anisotropy_clamp;
};

struct WGPUU32Array {
  const uint32_t *bytes;
  uintptr_t length;
};

struct WGPUShaderModuleDescriptor {
  struct WGPUU32Array code;
};

struct WGPUExtent3d {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
};

typedef uint32_t WGPUTextureUsage;
#define WGPUTextureUsage_COPY_SRC (uint32_t)1
#define WGPUTextureUsage_COPY_DST (uint32_t)2
#define WGPUTextureUsage_SAMPLED (uint32_t)4
#define WGPUTextureUsage_STORAGE (uint32_t)8
#define WGPUTextureUsage_OUTPUT_ATTACHMENT (uint32_t)16

struct WGPUTextureDescriptor {
  WGPURawString label;
  struct WGPUExtent3d size;
  uint32_t mip_level_count;
  uint32_t sample_count;
  enum WGPUTextureDimension dimension;
  enum WGPUTextureFormat format;
  WGPUTextureUsage usage;
};

struct WGPUTextureDataLayout {
  WGPUBufferAddress offset;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
};

struct WGPUBufferCopyView {
  WGPUBufferId buffer;
  struct WGPUTextureDataLayout layout;
};

struct WGPUOrigin3d {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};
#define WGPUOrigin3d_ZERO (WGPUOrigin3d){ .x = 0, .y = 0, .z = 0 }

struct WGPUTextureCopyView {
  WGPUTextureId texture;
  uint32_t mip_level;
  struct WGPUOrigin3d origin;
};

struct WGPUCommandBufferDescriptor {
  uint32_t todo;
};

struct WGPURequestAdapterOptions {
  enum WGPUPowerPreference power_preference;
  WGPUOption_SurfaceId compatible_surface;
};

typedef void *WGPUFactoryParam;

typedef WGPUNonZeroU64 WGPUId_SwapChain_Dummy;

typedef WGPUId_SwapChain_Dummy WGPUSwapChainId;

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
  void (*free_render_pipeline)(WGPURenderPipelineId, WGPUFactoryParam);
  void (*free_compute_pipeline)(WGPUComputePipelineId, WGPUFactoryParam);
  void (*free_buffer)(WGPUBufferId, WGPUFactoryParam);
  void (*free_texture)(WGPUTextureId, WGPUFactoryParam);
  void (*free_texture_view)(WGPUTextureViewId, WGPUFactoryParam);
  void (*free_sampler)(WGPUSamplerId, WGPUFactoryParam);
  void (*free_surface)(WGPUSurfaceId, WGPUFactoryParam);
};

typedef WGPUDeviceId WGPUQueueId;

struct WGPUTextureViewDescriptor {
  WGPURawString label;
  enum WGPUTextureFormat format;
  enum WGPUTextureViewDimension dimension;
  enum WGPUTextureAspect aspect;
  uint32_t base_mip_level;
  uint32_t level_count;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
};































WGPU_INLINE
WGPUBufferSize make_buffer_size(uint64_t aRawSize)
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

WGPU_INLINE
void wgpu_client_kill_adapter_id(const struct WGPUClient *aClient,
                                 WGPUAdapterId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_bind_group_id(const struct WGPUClient *aClient,
                                    WGPUBindGroupId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_bind_group_layout_id(const struct WGPUClient *aClient,
                                           WGPUBindGroupLayoutId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_buffer_id(const struct WGPUClient *aClient,
                                WGPUBufferId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_compute_pipeline_id(const struct WGPUClient *aClient,
                                          WGPUComputePipelineId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_device_id(const struct WGPUClient *aClient,
                                WGPUDeviceId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_encoder_id(const struct WGPUClient *aClient,
                                 WGPUCommandEncoderId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_pipeline_layout_id(const struct WGPUClient *aClient,
                                         WGPUPipelineLayoutId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_render_pipeline_id(const struct WGPUClient *aClient,
                                         WGPURenderPipelineId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_sampler_id(const struct WGPUClient *aClient,
                                 WGPUSamplerId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_shader_module_id(const struct WGPUClient *aClient,
                                       WGPUShaderModuleId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_texture_id(const struct WGPUClient *aClient,
                                 WGPUTextureId aId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_client_kill_texture_view_id(const struct WGPUClient *aClient,
                                      WGPUTextureViewId aId)
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
WGPUBindGroupId wgpu_client_make_bind_group_id(const struct WGPUClient *aClient,
                                               WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUBindGroupLayoutId wgpu_client_make_bind_group_layout_id(const struct WGPUClient *aClient,
                                                            WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUBufferId wgpu_client_make_buffer_id(const struct WGPUClient *aClient,
                                        WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUComputePipelineId wgpu_client_make_compute_pipeline_id(const struct WGPUClient *aClient,
                                                           WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUDeviceId wgpu_client_make_device_id(const struct WGPUClient *aClient,
                                        WGPUAdapterId aAdapterId)
WGPU_FUNC;

WGPU_INLINE
WGPUCommandEncoderId wgpu_client_make_encoder_id(const struct WGPUClient *aClient,
                                                 WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUPipelineLayoutId wgpu_client_make_pipeline_layout_id(const struct WGPUClient *aClient,
                                                         WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPURenderPipelineId wgpu_client_make_render_pipeline_id(const struct WGPUClient *aClient,
                                                         WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUSamplerId wgpu_client_make_sampler_id(const struct WGPUClient *aClient,
                                          WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUShaderModuleId wgpu_client_make_shader_module_id(const struct WGPUClient *aClient,
                                                     WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUTextureId wgpu_client_make_texture_id(const struct WGPUClient *aClient,
                                          WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
WGPUTextureViewId wgpu_client_make_texture_view_id(const struct WGPUClient *aClient,
                                                   WGPUDeviceId aDeviceId)
WGPU_FUNC;

WGPU_INLINE
struct WGPUInfrastructure wgpu_client_new(void)
WGPU_FUNC;

WGPU_INLINE
struct WGPURawPass wgpu_command_encoder_begin_compute_pass(WGPUCommandEncoderId aEncoderId,
                                                           const struct WGPUComputePassDescriptor *aDesc)
WGPU_FUNC;

WGPU_INLINE
struct WGPURawPass wgpu_command_encoder_begin_render_pass(WGPUCommandEncoderId aEncoderId,
                                                          const struct WGPURenderPassDescriptor *aDesc)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_destroy(struct WGPURawPass aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_dispatch(struct WGPURawPass *aPass,
                                uint32_t aGroupsX,
                                uint32_t aGroupsY,
                                uint32_t aGroupsZ)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_dispatch_indirect(struct WGPURawPass *aPass,
                                         WGPUBufferId aBufferId,
                                         WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
const uint8_t *wgpu_compute_pass_finish(struct WGPURawPass *aPass,
                                        uintptr_t *aLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_insert_debug_marker(struct WGPURawPass *aPass,
                                           WGPURawString aLabel)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_pop_debug_group(struct WGPURawPass *aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_push_debug_group(struct WGPURawPass *aPass,
                                        WGPURawString aLabel)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `offset_length` elements.
 */
WGPU_INLINE
void wgpu_compute_pass_set_bind_group(struct WGPURawPass *aPass,
                                      uint32_t aIndex,
                                      WGPUBindGroupId aBindGroupId,
                                      const WGPUDynamicOffset *aOffsets,
                                      uintptr_t aOffsetLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_compute_pass_set_pipeline(struct WGPURawPass *aPass,
                                    WGPUComputePipelineId aPipelineId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_destroy(struct WGPURawPass aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw(struct WGPURawPass *aPass,
                           uint32_t aVertexCount,
                           uint32_t aInstanceCount,
                           uint32_t aFirstVertex,
                           uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indexed(struct WGPURawPass *aPass,
                                   uint32_t aIndexCount,
                                   uint32_t aInstanceCount,
                                   uint32_t aFirstIndex,
                                   int32_t aBaseVertex,
                                   uint32_t aFirstInstance)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indexed_indirect(struct WGPURawPass *aPass,
                                            WGPUBufferId aBufferId,
                                            WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_draw_indirect(struct WGPURawPass *aPass,
                                    WGPUBufferId aBufferId,
                                    WGPUBufferAddress aOffset)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_execute_bundles(struct WGPURawPass *aPass,
                                      const WGPURenderBundleId *aBundles,
                                      uintptr_t aBundlesLength)
WGPU_FUNC;

WGPU_INLINE
const uint8_t *wgpu_render_pass_finish(struct WGPURawPass *aPass,
                                       uintptr_t *aLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_insert_debug_marker(struct WGPURawPass *aPass,
                                          WGPURawString aLabel)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_pop_debug_group(struct WGPURawPass *aPass)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_push_debug_group(struct WGPURawPass *aPass,
                                       WGPURawString aLabel)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `offset_length` elements.
 */
WGPU_INLINE
void wgpu_render_pass_set_bind_group(struct WGPURawPass *aPass,
                                     uint32_t aIndex,
                                     WGPUBindGroupId aBindGroupId,
                                     const WGPUDynamicOffset *aOffsets,
                                     uintptr_t aOffsetLength)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_blend_color(struct WGPURawPass *aPass,
                                      const struct WGPUColor *aColor)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_index_buffer(struct WGPURawPass *aPass,
                                       WGPUBufferId aBufferId,
                                       WGPUBufferAddress aOffset,
                                       WGPUBufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_pipeline(struct WGPURawPass *aPass,
                                   WGPURenderPipelineId aPipelineId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_scissor_rect(struct WGPURawPass *aPass,
                                       uint32_t aX,
                                       uint32_t aY,
                                       uint32_t aW,
                                       uint32_t aH)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_stencil_reference(struct WGPURawPass *aPass,
                                            uint32_t aValue)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_vertex_buffer(struct WGPURawPass *aPass,
                                        uint32_t aSlot,
                                        WGPUBufferId aBufferId,
                                        WGPUBufferAddress aOffset,
                                        WGPUBufferSize aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_render_pass_set_viewport(struct WGPURawPass *aPass,
                                   float aX,
                                   float aY,
                                   float aW,
                                   float aH,
                                   float aDepthMin,
                                   float aDepthMax)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_adapter_destroy(const struct WGPUGlobal *aGlobal,
                                 WGPUAdapterId aAdapterId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_adapter_request_device(const struct WGPUGlobal *aGlobal,
                                        WGPUAdapterId aSelfId,
                                        const struct WGPUDeviceDescriptor *aDesc,
                                        WGPUDeviceId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_bind_group_destroy(const struct WGPUGlobal *aGlobal,
                                    WGPUBindGroupId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_bind_group_layout_destroy(const struct WGPUGlobal *aGlobal,
                                           WGPUBindGroupLayoutId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_destroy(const struct WGPUGlobal *aGlobal,
                                WGPUBufferId aSelfId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `size` elements.
 */
WGPU_INLINE
const uint8_t *wgpu_server_buffer_get_mapped_range(const struct WGPUGlobal *aGlobal,
                                                   WGPUBufferId aBufferId,
                                                   WGPUBufferAddress aStart,
                                                   WGPUBufferAddress aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_map(const struct WGPUGlobal *aGlobal,
                            WGPUBufferId aBufferId,
                            WGPUBufferAddress aStart,
                            WGPUBufferAddress aSize,
                            struct WGPUBufferMapOperation aOperation)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_buffer_unmap(const struct WGPUGlobal *aGlobal,
                              WGPUBufferId aBufferId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `byte_length` elements.
 */
WGPU_INLINE
void wgpu_server_command_buffer_destroy(const struct WGPUGlobal *aGlobal,
                                        WGPUCommandBufferId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_compute_pipeline_destroy(const struct WGPUGlobal *aGlobal,
                                          WGPUComputePipelineId aSelfId)
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
void wgpu_server_device_create_bind_group(const struct WGPUGlobal *aGlobal,
                                          WGPUDeviceId aSelfId,
                                          const struct WGPUBindGroupDescriptor *aDesc,
                                          WGPUBindGroupId aNewId)
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
void wgpu_server_device_create_buffer(const struct WGPUGlobal *aGlobal,
                                      WGPUDeviceId aSelfId,
                                      const struct WGPUBufferDescriptor *aDesc,
                                      WGPUBufferId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_compute_pipeline(const struct WGPUGlobal *aGlobal,
                                                WGPUDeviceId aSelfId,
                                                const struct WGPUComputePipelineDescriptor *aDesc,
                                                WGPUComputePipelineId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_encoder(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUCommandEncoderDescriptor *aDesc,
                                       WGPUCommandEncoderId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_pipeline_layout(const struct WGPUGlobal *aGlobal,
                                               WGPUDeviceId aSelfId,
                                               const struct WGPUPipelineLayoutDescriptor *aDesc,
                                               WGPUPipelineLayoutId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_render_pipeline(const struct WGPUGlobal *aGlobal,
                                               WGPUDeviceId aSelfId,
                                               const struct WGPURenderPipelineDescriptor *aDesc,
                                               WGPURenderPipelineId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_sampler(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUSamplerDescriptor *aDesc,
                                       WGPUSamplerId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_shader_module(const struct WGPUGlobal *aGlobal,
                                             WGPUDeviceId aSelfId,
                                             const struct WGPUShaderModuleDescriptor *aDesc,
                                             WGPUShaderModuleId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_create_texture(const struct WGPUGlobal *aGlobal,
                                       WGPUDeviceId aSelfId,
                                       const struct WGPUTextureDescriptor *aDesc,
                                       WGPUTextureId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_device_destroy(const struct WGPUGlobal *aGlobal,
                                WGPUDeviceId aSelfId)
WGPU_FUNC;

/**
 * # Safety
 *
 * This function is unsafe as there is no guarantee that the given pointer is
 * valid for `size` elements.
 */
WGPU_INLINE
void wgpu_server_device_set_buffer_sub_data(const struct WGPUGlobal *aGlobal,
                                            WGPUDeviceId aSelfId,
                                            WGPUBufferId aBufferId,
                                            WGPUBufferAddress aOffset,
                                            const uint8_t *aData,
                                            WGPUBufferAddress aSize)
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
                                     const uint8_t *aBytes,
                                     uintptr_t aByteLength)
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
                                    const uint8_t *aCommands,
                                    uintptr_t aCommandLength)
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
void wgpu_server_encoder_copy_buffer_to_texture(const struct WGPUGlobal *aGlobal,
                                                WGPUCommandEncoderId aSelfId,
                                                const struct WGPUBufferCopyView *aSource,
                                                const struct WGPUTextureCopyView *aDestination,
                                                const struct WGPUExtent3d *aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_texture_to_buffer(const struct WGPUGlobal *aGlobal,
                                                WGPUCommandEncoderId aSelfId,
                                                const struct WGPUTextureCopyView *aSource,
                                                const struct WGPUBufferCopyView *aDestination,
                                                const struct WGPUExtent3d *aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_copy_texture_to_texture(const struct WGPUGlobal *aGlobal,
                                                 WGPUCommandEncoderId aSelfId,
                                                 const struct WGPUTextureCopyView *aSource,
                                                 const struct WGPUTextureCopyView *aDestination,
                                                 const struct WGPUExtent3d *aSize)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_destroy(const struct WGPUGlobal *aGlobal,
                                 WGPUCommandEncoderId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_encoder_finish(const struct WGPUGlobal *aGlobal,
                                WGPUCommandEncoderId aSelfId,
                                const struct WGPUCommandBufferDescriptor *aDesc)
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
                                            const struct WGPURequestAdapterOptions *aDesc,
                                            const WGPUAdapterId *aIds,
                                            uintptr_t aIdLength)
WGPU_FUNC;

WGPU_INLINE
struct WGPUGlobal *wgpu_server_new(struct WGPUIdentityRecyclerFactory aFactory)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_pipeline_layout_destroy(const struct WGPUGlobal *aGlobal,
                                         WGPUPipelineLayoutId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_poll_all_devices(const struct WGPUGlobal *aGlobal,
                                  bool aForceWait)
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
                                     const struct WGPUTextureCopyView *aDestination,
                                     const uint8_t *aData,
                                     uintptr_t aDataLength,
                                     const struct WGPUTextureDataLayout *aLayout,
                                     const struct WGPUExtent3d *aExtent)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_render_pipeline_destroy(const struct WGPUGlobal *aGlobal,
                                         WGPURenderPipelineId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_sampler_destroy(const struct WGPUGlobal *aGlobal,
                                 WGPUSamplerId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_shader_module_destroy(const struct WGPUGlobal *aGlobal,
                                       WGPUShaderModuleId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_create_view(const struct WGPUGlobal *aGlobal,
                                     WGPUTextureId aSelfId,
                                     const struct WGPUTextureViewDescriptor *aDesc,
                                     WGPUTextureViewId aNewId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_destroy(const struct WGPUGlobal *aGlobal,
                                 WGPUTextureId aSelfId)
WGPU_FUNC;

WGPU_INLINE
void wgpu_server_texture_view_destroy(const struct WGPUGlobal *aGlobal,
                                      WGPUTextureViewId aSelfId)
WGPU_FUNC;
