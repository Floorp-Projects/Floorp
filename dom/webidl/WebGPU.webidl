/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://gpuweb.github.io/gpuweb/
 */



typedef [EnforceRange] unsigned long GPUBufferDynamicOffset;
typedef [EnforceRange] unsigned long long GPUFenceValue;
typedef [EnforceRange] unsigned long GPUStencilValue;
typedef [EnforceRange] unsigned long GPUSampleMask;
typedef [EnforceRange] long GPUDepthBias;

typedef [EnforceRange] unsigned long long GPUSize64;
typedef [EnforceRange] unsigned long GPUIntegerCoordinate;
typedef [EnforceRange] unsigned long GPUIndex32;
typedef [EnforceRange] unsigned long GPUSize32;
typedef [EnforceRange] long GPUSignedOffset32;

dictionary GPUColorDict {
    required double r;
    required double g;
    required double b;
    required double a;
};

dictionary GPUOrigin2DDict {
    GPUIntegerCoordinate x = 0;
    GPUIntegerCoordinate y = 0;
};

dictionary GPUOrigin3DDict {
    GPUIntegerCoordinate x = 0;
    GPUIntegerCoordinate y = 0;
    GPUIntegerCoordinate z = 0;
};

dictionary GPUExtent3DDict {
    required GPUIntegerCoordinate width;
    required GPUIntegerCoordinate height;
    required GPUIntegerCoordinate depth;
};

typedef (sequence<double> or GPUColorDict) GPUColor;
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin2DDict) GPUOrigin2D;
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin3DDict) GPUOrigin3D;
typedef (sequence<GPUIntegerCoordinate> or GPUExtent3DDict) GPUExtent3D;

interface mixin GPUObjectBase {
    attribute DOMString? label;
};

dictionary GPUObjectDescriptorBase {
    DOMString? label;
};

// ****************************************************************************
// INITIALIZATION
// ****************************************************************************

[
    Pref="dom.webgpu.enabled",
    Exposed=Window
]
interface GPU {
    // May reject with DOMException
    [NewObject]
    Promise<GPUAdapter> requestAdapter(optional GPURequestAdapterOptions options = {});
};

// Add a "webgpu" member to Navigator/Worker that contains the global instance of a "WebGPU"
interface mixin GPUProvider {
    [SameObject, Replaceable, Pref="dom.webgpu.enabled", Exposed=Window] readonly attribute GPU gpu;
};

enum GPUPowerPreference {
    "low-power",
    "high-performance"
};

dictionary GPURequestAdapterOptions {
    GPUPowerPreference powerPreference;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUAdapter {
    readonly attribute DOMString name;
    //GPUExtensions getExtensions();
    //readonly attribute GPULimits limits; Don't expose higher limits for now.

    // May reject with DOMException
    [NewObject]
    Promise<GPUDevice> requestDevice(optional GPUDeviceDescriptor descriptor = {});
};
GPUAdapter includes GPUObjectBase;

dictionary GPUExtensions {
    boolean anisotropicFiltering = false;
};

dictionary GPULimits {
    GPUSize32 maxBindGroups = 4;
    GPUSize32 maxDynamicUniformBuffersPerPipelineLayout = 8;
    GPUSize32 maxDynamicStorageBuffersPerPipelineLayout = 4;
    GPUSize32 maxSampledTexturesPerShaderStage = 16;
    GPUSize32 maxSamplersPerShaderStage = 16;
    GPUSize32 maxStorageBuffersPerShaderStage = 4;
    GPUSize32 maxStorageTexturesPerShaderStage = 4;
    GPUSize32 maxUniformBuffersPerShaderStage = 12;
    GPUSize32 maxUniformBufferBindingSize = 16384;
};

// Device
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUDevice {
    //GPUExtensions getExtensions();
    //GPULimits getLimits();
    //readonly attribute GPUAdapter adapter;

    [SameObject] readonly attribute GPUQueue defaultQueue;

    [NewObject]
    GPUBuffer createBuffer(GPUBufferDescriptor descriptor);
    [NewObject, Throws]
    GPUMappedBuffer createBufferMapped(GPUBufferDescriptor descriptor);
    [NewObject]
    GPUTexture createTexture(GPUTextureDescriptor descriptor);
    [NewObject]
    GPUSampler createSampler(optional GPUSamplerDescriptor descriptor = {});

    GPUBindGroupLayout createBindGroupLayout(GPUBindGroupLayoutDescriptor descriptor);
    GPUPipelineLayout createPipelineLayout(GPUPipelineLayoutDescriptor descriptor);
    GPUBindGroup createBindGroup(GPUBindGroupDescriptor descriptor);

    GPUShaderModule createShaderModule(GPUShaderModuleDescriptor descriptor);
    GPUComputePipeline createComputePipeline(GPUComputePipelineDescriptor descriptor);
    GPURenderPipeline createRenderPipeline(GPURenderPipelineDescriptor descriptor);

    [NewObject]
    GPUCommandEncoder createCommandEncoder(optional GPUCommandEncoderDescriptor descriptor = {});
    //GPURenderBundleEncoder createRenderBundleEncoder(GPURenderBundleEncoderDescriptor descriptor);
};
GPUDevice includes GPUObjectBase;

dictionary GPUDeviceDescriptor {
    GPUExtensions extensions;
    GPULimits limits;

    // TODO are other things configurable like queues?
};


// ****************************************************************************
// ERROR HANDLING
// ****************************************************************************

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUDeviceLostInfo {
    readonly attribute DOMString message;
};

enum GPUErrorFilter {
    "none",
    "out-of-memory",
    "validation"
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUOutOfMemoryError {
    //constructor();
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUValidationError {
    //constructor(DOMString message);
    //readonly attribute DOMString message;
};

typedef (GPUOutOfMemoryError or GPUValidationError) GPUError;

partial interface GPUDevice {
    //readonly attribute Promise<GPUDeviceLostInfo> lost;
    //void pushErrorScope(GPUErrorFilter filter);
    //Promise<GPUError?> popErrorScope();
    //[Exposed=Window]
    //attribute EventHandler onuncapturederror;
};

// ****************************************************************************
// SHADER RESOURCES (buffer, textures, texture views, samples)
// ****************************************************************************

// Buffer
typedef unsigned long GPUBufferUsageFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUBufferUsage {
    const GPUBufferUsageFlags MAP_READ      = 0x0001;
    const GPUBufferUsageFlags MAP_WRITE     = 0x0002;
    const GPUBufferUsageFlags COPY_SRC      = 0x0004;
    const GPUBufferUsageFlags COPY_DST      = 0x0008;
    const GPUBufferUsageFlags INDEX         = 0x0010;
    const GPUBufferUsageFlags VERTEX        = 0x0020;
    const GPUBufferUsageFlags UNIFORM       = 0x0040;
    const GPUBufferUsageFlags STORAGE       = 0x0080;
    const GPUBufferUsageFlags INDIRECT      = 0x0100;
    const GPUBufferUsageFlags QUERY_RESOLVE = 0x0200;
};

dictionary GPUBufferDescriptor {
    required GPUSize64 size;
    required GPUBufferUsageFlags usage;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUBuffer {
    [NewObject]
    Promise<ArrayBuffer> mapReadAsync();
    //Promise<ArrayBuffer> mapWriteAsync();
    [Throws]
    void unmap();

    void destroy();
};
GPUBuffer includes GPUObjectBase;

typedef sequence<any> GPUMappedBuffer;

// Texture
enum GPUTextureDimension {
    "1d",
    "2d",
    "3d",
};

enum GPUTextureFormat {
    // 8-bit formats
    "r8unorm",
    "r8snorm",
    "r8uint",
    "r8sint",

    // 16-bit formats
    "r16uint",
    "r16sint",
    "r16float",
    "rg8unorm",
    "rg8snorm",
    "rg8uint",
    "rg8sint",

    // 32-bit formats
    "r32uint",
    "r32sint",
    "r32float",
    "rg16uint",
    "rg16sint",
    "rg16float",
    "rgba8unorm",
    "rgba8unorm-srgb",
    "rgba8snorm",
    "rgba8uint",
    "rgba8sint",
    "bgra8unorm",
    "bgra8unorm-srgb",
    // Packed 32-bit formats
    "rgb10a2unorm",
    "rg11b10float",

    // 64-bit formats
    "rg32uint",
    "rg32sint",
    "rg32float",
    "rgba16uint",
    "rgba16sint",
    "rgba16float",

    // 128-bit formats
    "rgba32uint",
    "rgba32sint",
    "rgba32float",

    // Depth and stencil formats
    "depth32float",
    "depth24plus",
    "depth24plus-stencil8"
};

typedef unsigned long GPUTextureUsageFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUTextureUsage {
    const GPUTextureUsageFlags COPY_SRC          = 0x01;
    const GPUTextureUsageFlags COPY_DST          = 0x02;
    const GPUTextureUsageFlags SAMPLED           = 0x04;
    const GPUTextureUsageFlags STORAGE           = 0x08;
    const GPUTextureUsageFlags OUTPUT_ATTACHMENT = 0x10;
};

dictionary GPUTextureDescriptor {
    required GPUExtent3D size;
    GPUIntegerCoordinate mipLevelCount = 1;
    GPUSize32 sampleCount = 1;
    GPUTextureDimension dimension = "2d";
    required GPUTextureFormat format;
    required GPUTextureUsageFlags usage;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUTexture {
    [NewObject]
    GPUTextureView createView(optional GPUTextureViewDescriptor descriptor = {});

    void destroy();
};
GPUTexture includes GPUObjectBase;

// Texture view
enum GPUTextureViewDimension {
    "1d",
    "2d",
    "2d-array",
    "cube",
    "cube-array",
    "3d"
};

enum GPUTextureAspect {
    "all",
    "stencil-only",
    "depth-only"
};

dictionary GPUTextureViewDescriptor : GPUObjectDescriptorBase {
    GPUTextureFormat format;
    GPUTextureViewDimension dimension;
    GPUTextureAspect aspect = "all";
    GPUIntegerCoordinate baseMipLevel = 0;
    GPUIntegerCoordinate mipLevelCount = 1;
    GPUIntegerCoordinate baseArrayLayer = 0;
    GPUIntegerCoordinate arrayLayerCount = 1;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUTextureView {
};
GPUTextureView includes GPUObjectBase;

// Sampler
enum GPUAddressMode {
    "clamp-to-edge",
    "repeat",
    "mirror-repeat"
};

enum GPUFilterMode {
    "nearest",
    "linear",
};

enum GPUCompareFunction {
    "never",
    "less",
    "equal",
    "less-equal",
    "greater",
    "not-equal",
    "greater-equal",
    "always"
};

dictionary GPUSamplerDescriptor : GPUObjectDescriptorBase {
    GPUAddressMode addressModeU = "clamp-to-edge";
    GPUAddressMode addressModeV = "clamp-to-edge";
    GPUAddressMode addressModeW = "clamp-to-edge";
    GPUFilterMode magFilter = "nearest";
    GPUFilterMode minFilter = "nearest";
    GPUFilterMode mipmapFilter = "nearest";
    float lodMinClamp = 0;
    float lodMaxClamp = 1000.0; //TODO?
    GPUCompareFunction compare;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUSampler {
};
GPUSampler includes GPUObjectBase;

enum GPUTextureComponentType {
    "float",
    "sint",
    "uint"
};

// ****************************************************************************
// BINDING MODEL (bindgroup layout, bindgroup)
// ****************************************************************************

// PipelineLayout
dictionary GPUPipelineLayoutDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUBindGroupLayout> bindGroupLayouts;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUPipelineLayout {
};
GPUPipelineLayout includes GPUObjectBase;

// BindGroupLayout
typedef unsigned long GPUShaderStageFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUShaderStage {
    const GPUShaderStageFlags VERTEX = 1;
    const GPUShaderStageFlags FRAGMENT = 2;
    const GPUShaderStageFlags COMPUTE = 4;
};

enum GPUBindingType {
    "uniform-buffer",
    "storage-buffer",
    "readonly-storage-buffer",
    "sampler",
    "comparison-sampler",
    "sampled-texture",
    "readonly-storage-texture",
    "writeonly-storage-texture",
};

dictionary GPUBindGroupLayoutEntry {
    required GPUIndex32 binding;
    required GPUShaderStageFlags visibility;
    required GPUBindingType type;
    GPUTextureViewDimension viewDimension = "2d";
    GPUTextureComponentType textureComponentType = "float";
    boolean multisampled = false;
    boolean hasDynamicOffset = false;
    GPUTextureFormat storageTextureFormat;
};

dictionary GPUBindGroupLayoutDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUBindGroupLayoutEntry> entries;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUBindGroupLayout {
};
GPUBindGroupLayout includes GPUObjectBase;

// BindGroup
dictionary GPUBufferBinding {
    required GPUBuffer buffer;
    GPUSize64 offset = 0;
    GPUSize64 size;
};

typedef (GPUSampler or GPUTextureView or GPUBufferBinding) GPUBindingResource;

dictionary GPUBindGroupEntry {
    required GPUIndex32 binding;
    required GPUBindingResource resource;
};

dictionary GPUBindGroupDescriptor : GPUObjectDescriptorBase {
    required GPUBindGroupLayout layout;
    required sequence<GPUBindGroupEntry> entries;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUBindGroup {
};
GPUBindGroup includes GPUObjectBase;

// ****************************************************************************
// PIPELINE CREATION (blend state, DS state, ..., pipelines)
// ****************************************************************************

// BlendState
enum GPUBlendFactor {
    "zero",
    "one",
    "src-color",
    "one-minus-src-color",
    "src-alpha",
    "one-minus-src-alpha",
    "dst-color",
    "one-minus-dst-color",
    "dst-alpha",
    "one-minus-dst-alpha",
    "src-alpha-saturated",
    "blend-color",
    "one-minus-blend-color",
};

enum GPUBlendOperation {
    "add",
    "subtract",
    "reverse-subtract",
    "min",
    "max"
};

dictionary GPUBlendDescriptor {
    GPUBlendFactor srcFactor = "one";
    GPUBlendFactor dstFactor = "zero";
    GPUBlendOperation operation = "add";
};

typedef unsigned long GPUColorWriteFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUColorWrite {
    const GPUColorWriteFlags RED    = 0x1;
    const GPUColorWriteFlags GREEN  = 0x2;
    const GPUColorWriteFlags BLUE   = 0x4;
    const GPUColorWriteFlags ALPHA  = 0x8;
    const GPUColorWriteFlags ALL    = 0xF;
};

dictionary GPUColorStateDescriptor {
    required GPUTextureFormat format;

    GPUBlendDescriptor alphaBlend = {};
    GPUBlendDescriptor colorBlend = {};
    GPUColorWriteFlags writeMask = 0xF;
};

// DepthStencilState
enum GPUStencilOperation {
    "keep",
    "zero",
    "replace",
    "invert",
    "increment-clamp",
    "decrement-clamp",
    "increment-wrap",
    "decrement-wrap"
};

dictionary GPUStencilStateFaceDescriptor {
    GPUCompareFunction compare = "always";
    GPUStencilOperation failOp = "keep";
    GPUStencilOperation depthFailOp = "keep";
    GPUStencilOperation passOp = "keep";
};

dictionary GPUDepthStencilStateDescriptor {
    required GPUTextureFormat format;

    boolean depthWriteEnabled = false;
    GPUCompareFunction depthCompare = "always";

    GPUStencilStateFaceDescriptor stencilFront = {};
    GPUStencilStateFaceDescriptor stencilBack = {};

    GPUStencilValue stencilReadMask = 0xFFFFFFFF;
    GPUStencilValue stencilWriteMask = 0xFFFFFFFF;
};

// InputState
enum GPUIndexFormat {
    "uint16",
    "uint32",
};

enum GPUVertexFormat {
    "uchar2",
    "uchar4",
    "char2",
    "char4",
    "uchar2norm",
    "uchar4norm",
    "char2norm",
    "char4norm",
    "ushort2",
    "ushort4",
    "short2",
    "short4",
    "ushort2norm",
    "ushort4norm",
    "short2norm",
    "short4norm",
    "half2",
    "half4",
    "float",
    "float2",
    "float3",
    "float4",
    "uint",
    "uint2",
    "uint3",
    "uint4",
    "int",
    "int2",
    "int3",
    "int4",
};

enum GPUInputStepMode {
    "vertex",
    "instance",
};

dictionary GPUVertexAttributeDescriptor {
    required GPUVertexFormat format;
    required GPUSize64 offset;
    required GPUIndex32 shaderLocation;
};

dictionary GPUVertexBufferLayoutDescriptor {
    required GPUSize64 arrayStride;
    GPUInputStepMode stepMode = "vertex";
    required sequence<GPUVertexAttributeDescriptor> attributes;
};

dictionary GPUVertexStateDescriptor {
    GPUIndexFormat indexFormat = "uint32";
    sequence<GPUVertexBufferLayoutDescriptor?> vertexBuffers = [];
};

// ShaderModule
typedef (Uint32Array or DOMString) GPUShaderCode;

dictionary GPUShaderModuleDescriptor : GPUObjectDescriptorBase {
    required GPUShaderCode code;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUShaderModule {
};
GPUShaderModule includes GPUObjectBase;

// Common stuff for ComputePipeline and RenderPipeline
dictionary GPUPipelineDescriptorBase : GPUObjectDescriptorBase {
    required GPUPipelineLayout layout;
};

dictionary GPUProgrammableStageDescriptor {
    required GPUShaderModule module;
    required DOMString entryPoint;
};

// ComputePipeline
dictionary GPUComputePipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUProgrammableStageDescriptor computeStage;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUComputePipeline {
};
GPUComputePipeline includes GPUObjectBase;

// GPURenderPipeline
enum GPUPrimitiveTopology {
    "point-list",
    "line-list",
    "line-strip",
    "triangle-list",
    "triangle-strip"
};

dictionary GPURasterizationStateDescriptor {
    GPUFrontFace frontFace = "ccw";
    GPUCullMode cullMode = "none";

    GPUDepthBias depthBias = 0;
    float depthBiasSlopeScale = 0;
    float depthBiasClamp = 0;
};

enum GPUFrontFace {
    "ccw",
    "cw"
};

enum GPUCullMode {
    "none",
    "front",
    "back"
};

dictionary GPURenderPipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUProgrammableStageDescriptor vertexStage;
    GPUProgrammableStageDescriptor fragmentStage;

    required GPUPrimitiveTopology primitiveTopology;
    GPURasterizationStateDescriptor rasterizationState = {};
    required sequence<GPUColorStateDescriptor> colorStates;
    GPUDepthStencilStateDescriptor depthStencilState;
    GPUVertexStateDescriptor vertexState = {};

    GPUSize32 sampleCount = 1;
    GPUSampleMask sampleMask = 0xFFFFFFFF;
    boolean alphaToCoverageEnabled = false;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderPipeline {
};
GPURenderPipeline includes GPUObjectBase;

// ****************************************************************************
// COMMAND RECORDING (Command buffer and all relevant structures)
// ****************************************************************************

enum GPULoadOp {
    "load"
};

enum GPUStoreOp {
    "store",
    "clear"
};

dictionary GPURenderPassColorAttachmentDescriptor {
    required GPUTextureView attachment;
    GPUTextureView resolveTarget;

    required (GPULoadOp or GPUColor) loadValue;
    GPUStoreOp storeOp = "store";
};

dictionary GPURenderPassDepthStencilAttachmentDescriptor {
    required GPUTextureView attachment;

    required (GPULoadOp or float) depthLoadValue;
    required GPUStoreOp depthStoreOp;

    required (GPULoadOp or GPUStencilValue) stencilLoadValue;
    required GPUStoreOp stencilStoreOp;
};

dictionary GPURenderPassDescriptor : GPUObjectDescriptorBase {
    required sequence<GPURenderPassColorAttachmentDescriptor> colorAttachments;
    GPURenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
};

dictionary GPUTextureDataLayout {
    GPUSize64 offset = 0;
    required GPUSize32 bytesPerRow;
    GPUSize32 rowsPerImage = 0;
};

dictionary GPUBufferCopyView : GPUTextureDataLayout {
    required GPUBuffer buffer;
};

dictionary GPUTextureCopyView {
    required GPUTexture texture;
    GPUIntegerCoordinate mipLevel = 0;
    GPUSize32 arrayLayer = 0;
    GPUOrigin3D origin;
};

dictionary GPUImageBitmapCopyView {
    //required ImageBitmap imageBitmap; //TODO
    GPUOrigin2D origin;
};

dictionary GPUCommandEncoderDescriptor : GPUObjectDescriptorBase {
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUCommandEncoder {
    [NewObject]
    GPUComputePassEncoder beginComputePass(optional GPUComputePassDescriptor descriptor = {});
    [NewObject]
    GPURenderPassEncoder beginRenderPass(GPURenderPassDescriptor descriptor);

    void copyBufferToBuffer(
        GPUBuffer source,
        GPUSize64 sourceOffset,
        GPUBuffer destination,
        GPUSize64 destinationOffset,
        GPUSize64 size);

    void copyBufferToTexture(
        GPUBufferCopyView source,
        GPUTextureCopyView destination,
        GPUExtent3D copySize);

    void copyTextureToBuffer(
        GPUTextureCopyView source,
        GPUBufferCopyView destination,
        GPUExtent3D copySize);

    void copyTextureToTexture(
        GPUTextureCopyView source,
        GPUTextureCopyView destination,
        GPUExtent3D copySize);

    /*
    void copyImageBitmapToTexture(
        GPUImageBitmapCopyView source,
        GPUTextureCopyView destination,
        GPUExtent3D copySize);
    */

    //void pushDebugGroup(DOMString groupLabel);
    //void popDebugGroup();
    //void insertDebugMarker(DOMString markerLabel);

    [NewObject]
    GPUCommandBuffer finish(optional GPUCommandBufferDescriptor descriptor = {});
};
GPUCommandEncoder includes GPUObjectBase;

interface mixin GPUProgrammablePassEncoder {
    void setBindGroup(GPUIndex32 index, GPUBindGroup bindGroup,
                      optional sequence<GPUBufferDynamicOffset> dynamicOffsets = []);

    //void pushDebugGroup(DOMString groupLabel);
    //void popDebugGroup();
    //void insertDebugMarker(DOMString markerLabel);
};

// Render Pass
interface mixin GPURenderEncoderBase {
    void setPipeline(GPURenderPipeline pipeline);

    void setIndexBuffer(GPUBuffer buffer, optional GPUSize64 offset = 0, optional GPUSize64 size = 0);
    void setVertexBuffer(GPUIndex32 slot, GPUBuffer buffer, optional GPUSize64 offset = 0, optional GPUSize64 size = 0);

    void draw(GPUSize32 vertexCount,
              optional GPUSize32 instanceCount = 1,
              optional GPUSize32 firstVertex = 0,
              optional GPUSize32 firstInstance = 0);
    void drawIndexed(GPUSize32 indexCount,
                     optional GPUSize32 instanceCount = 1,
                     optional GPUSize32 firstIndex = 0,
                     optional GPUSignedOffset32 baseVertex = 0,
                     optional GPUSize32 firstInstance = 0);

    void drawIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);
    void drawIndexedIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderPassEncoder {
    //void setViewport(float x, float y,
    //                 float width, float height,
    //                 float minDepth, float maxDepth);

    //void setScissorRect(u32 x, u32 y, u32 width, u32 height);

    //void setBlendColor(GPUColor color);
    //void setStencilReference(u32 reference);

    //void executeBundles(sequence<GPURenderBundle> bundles);
    [Throws]
    void endPass();
};
GPURenderPassEncoder includes GPUObjectBase;
GPURenderPassEncoder includes GPUProgrammablePassEncoder;
GPURenderPassEncoder includes GPURenderEncoderBase;

// Compute Pass
dictionary GPUComputePassDescriptor : GPUObjectDescriptorBase {
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUComputePassEncoder {
    void setPipeline(GPUComputePipeline pipeline);
    void dispatch(GPUSize32 x, optional GPUSize32 y = 1, optional GPUSize32 z = 1);
    void dispatchIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);

    [Throws]
    void endPass();
};
GPUComputePassEncoder includes GPUObjectBase;
GPUComputePassEncoder includes GPUProgrammablePassEncoder;

// Command Buffer
dictionary GPUCommandBufferDescriptor : GPUObjectDescriptorBase {
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUCommandBuffer {
};
GPUCommandBuffer includes GPUObjectBase;

dictionary GPURenderBundleEncoderDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUTextureFormat> colorFormats;
    GPUTextureFormat depthStencilFormat;
    GPUSize32 sampleCount = 1;
};

// Render Bundle
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderBundleEncoder {
    //GPURenderBundle finish(optional GPURenderBundleDescriptor descriptor = {});
};
GPURenderBundleEncoder includes GPUObjectBase;
//GPURenderBundleEncoder includes GPURenderEncoderBase;

dictionary GPURenderBundleDescriptor : GPUObjectDescriptorBase {
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderBundle {
};
GPURenderBundle includes GPUObjectBase;

// ****************************************************************************
// OTHER (Fence, Queue SwapChain, Device)
// ****************************************************************************

// Fence
dictionary GPUFenceDescriptor : GPUObjectDescriptorBase {
    GPUFenceValue initialValue = 0;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUFence {
    //GPUFenceValue getCompletedValue();
    //Promise<void> onCompletion(GPUFenceValue completionValue);
};
GPUFence includes GPUObjectBase;

// Queue
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUQueue {
    void submit(sequence<GPUCommandBuffer> buffers);

    //GPUFence createFence(optional GPUFenceDescriptor descriptor = {});
    //void signal(GPUFence fence, GPUFenceValue signalValue);
};
GPUQueue includes GPUObjectBase;

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUSwapChain {
    GPUTexture getCurrentTexture();
};
GPUSwapChain includes GPUObjectBase;

dictionary GPUSwapChainDescriptor : GPUObjectDescriptorBase {
    required GPUDevice device;
    required GPUTextureFormat format;
    GPUTextureUsageFlags usage = 0x10; //GPUTextureUsage.OUTPUT_ATTACHMENT
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUCanvasContext {
    // Calling configureSwapChain a second time invalidates the previous one,
    // and all of the textures it's produced.
    [Throws]
    GPUSwapChain configureSwapChain(GPUSwapChainDescriptor descriptor);

    //Promise<GPUTextureFormat> getSwapChainPreferredFormat(GPUDevice device);
};
