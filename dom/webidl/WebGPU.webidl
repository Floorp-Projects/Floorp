/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://gpuweb.github.io/gpuweb/
 */



typedef [EnforceRange] unsigned long GPUBufferDynamicOffset;
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
    GPUIntegerCoordinate height = 1;
    GPUIntegerCoordinate depthOrArrayLayers = 1;
};

typedef (sequence<double> or GPUColorDict) GPUColor;
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin2DDict) GPUOrigin2D;
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin3DDict) GPUOrigin3D;
typedef (sequence<GPUIntegerCoordinate> or GPUExtent3DDict) GPUExtent3D;

interface mixin GPUObjectBase {
    attribute USVString? label;
};

dictionary GPUObjectDescriptorBase {
    USVString label;
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
    Promise<GPUAdapter?> requestAdapter(optional GPURequestAdapterOptions options = {});
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
interface GPUAdapterFeatures {
    readonly setlike<GPUFeatureName>;
};

dictionary GPUDeviceDescriptor {
    sequence<GPUFeatureName> nonGuaranteedFeatures = [];
    record<DOMString, GPUSize32> nonGuaranteedLimits;
};

enum GPUFeatureName {
    "depth-clamping",
    "depth24unorm-stencil8",
    "depth32float-stencil8",
    "pipeline-statistics-query",
    "texture-compression-bc",
    "timestamp-query",
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUAdapterLimits {
    readonly attribute unsigned long maxTextureDimension1D;
    readonly attribute unsigned long maxTextureDimension2D;
    readonly attribute unsigned long maxTextureDimension3D;
    readonly attribute unsigned long maxTextureArrayLayers;
    readonly attribute unsigned long maxBindGroups;
    readonly attribute unsigned long maxDynamicUniformBuffersPerPipelineLayout;
    readonly attribute unsigned long maxDynamicStorageBuffersPerPipelineLayout;
    readonly attribute unsigned long maxSampledTexturesPerShaderStage;
    readonly attribute unsigned long maxSamplersPerShaderStage;
    readonly attribute unsigned long maxStorageBuffersPerShaderStage;
    readonly attribute unsigned long maxStorageTexturesPerShaderStage;
    readonly attribute unsigned long maxUniformBuffersPerShaderStage;
    readonly attribute unsigned long maxUniformBufferBindingSize;
    readonly attribute unsigned long maxStorageBufferBindingSize;
    readonly attribute unsigned long maxVertexBuffers;
    readonly attribute unsigned long maxVertexAttributes;
    readonly attribute unsigned long maxVertexBufferArrayStride;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUAdapter {
    readonly attribute DOMString name;
    [SameObject] readonly attribute GPUAdapterFeatures features;
    [SameObject] readonly attribute GPUAdapterLimits limits;

    [NewObject]
    Promise<GPUDevice> requestDevice(optional GPUDeviceDescriptor descriptor = {});
};

// Device
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUDevice: EventTarget {
    //[SameObject] readonly attribute GPUAdapter adapter;
    //readonly attribute FrozenArray<GPUFeatureName> features;
    //readonly attribute object limits;

    // Overriding the name to avoid collision with `class Queue` in gcc
    [SameObject, BinaryName="getQueue"] readonly attribute GPUQueue queue;

    void destroy();

    [NewObject, Throws]
    GPUBuffer createBuffer(GPUBufferDescriptor descriptor);
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

    //Promise<GPUComputePipeline> createComputePipelineAsync(GPUComputePipelineDescriptor descriptor);
    //Promise<GPURenderPipeline> createRenderPipelineAsync(GPURenderPipelineDescriptor descriptor);

    [NewObject]
    GPUCommandEncoder createCommandEncoder(optional GPUCommandEncoderDescriptor descriptor = {});
    //[NewObject]
    //GPURenderBundleEncoder createRenderBundleEncoder(GPURenderBundleEncoderDescriptor descriptor);
    //[NewObject]
    //GPUQuerySet createQuerySet(GPUQuerySetDescriptor descriptor);
};
GPUDevice includes GPUObjectBase;


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
    constructor(DOMString message);
    readonly attribute DOMString message;
};

typedef (GPUOutOfMemoryError or GPUValidationError) GPUError;

partial interface GPUDevice {
    //readonly attribute Promise<GPUDeviceLostInfo> lost;
    //void pushErrorScope(GPUErrorFilter filter);
    //Promise<GPUError?> popErrorScope();
    [Exposed=Window]
    attribute EventHandler onuncapturederror;
};

// ****************************************************************************
// SHADER RESOURCES (buffer, textures, texture views, samples)
// ****************************************************************************

// Buffer
typedef [EnforceRange] unsigned long GPUBufferUsageFlags;
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

dictionary GPUBufferDescriptor : GPUObjectDescriptorBase {
    required GPUSize64 size;
    required GPUBufferUsageFlags usage;
    boolean mappedAtCreation = false;
};

typedef [EnforceRange] unsigned long GPUMapModeFlags;

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUMapMode
 {
    const GPUMapModeFlags READ  = 0x0001;
    const GPUMapModeFlags WRITE = 0x0002;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUBuffer {
    [NewObject]
    Promise<void> mapAsync(GPUMapModeFlags mode, optional GPUSize64 offset = 0, optional GPUSize64 size);
    [NewObject, Throws]
    ArrayBuffer getMappedRange(optional GPUSize64 offset = 0, optional GPUSize64 size);
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
    "stencil8",
    "depth16unorm",
    "depth24plus",
    "depth24plus-stencil8",
    "depth32float",

    // BC compressed formats usable if "texture-compression-bc" is both
    // supported by the device/user agent and enabled in requestDevice.
    "bc1-rgba-unorm",
    "bc1-rgba-unorm-srgb",
    "bc2-rgba-unorm",
    "bc2-rgba-unorm-srgb",
    "bc3-rgba-unorm",
    "bc3-rgba-unorm-srgb",
    "bc4-r-unorm",
    "bc4-r-snorm",
    "bc5-rg-unorm",
    "bc5-rg-snorm",
    "bc6h-rgb-ufloat",
    "bc6h-rgb-float",
    "bc7-rgba-unorm",
    "bc7-rgba-unorm-srgb",

    // "depth24unorm-stencil8" feature
    "depth24unorm-stencil8",

    // "depth32float-stencil8" feature
    "depth32float-stencil8",
};

typedef [EnforceRange] unsigned long GPUTextureUsageFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUTextureUsage {
    const GPUTextureUsageFlags COPY_SRC          = 0x01;
    const GPUTextureUsageFlags COPY_DST          = 0x02;
    const GPUTextureUsageFlags SAMPLED           = 0x04;
    const GPUTextureUsageFlags STORAGE           = 0x08;
    const GPUTextureUsageFlags RENDER_ATTACHMENT = 0x10;
};

dictionary GPUTextureDescriptor : GPUObjectDescriptorBase {
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
    GPUIntegerCoordinate mipLevelCount;
    GPUIntegerCoordinate baseArrayLayer = 0;
    GPUIntegerCoordinate arrayLayerCount;
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
    float lodMaxClamp = 1000.0; // TODO: What should this be?
    GPUCompareFunction compare;
    [Clamp] unsigned short maxAnisotropy = 1;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUSampler {
};
GPUSampler includes GPUObjectBase;

enum GPUTextureComponentType {
    "float",
    "sint",
    "uint",
    "depth-comparison"
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
typedef [EnforceRange] unsigned long GPUShaderStageFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUShaderStage {
    const GPUShaderStageFlags VERTEX = 1;
    const GPUShaderStageFlags FRAGMENT = 2;
    const GPUShaderStageFlags COMPUTE = 4;
};

enum GPUBufferBindingType {
    "uniform",
    "storage",
    "read-only-storage",
};

dictionary GPUBufferBindingLayout {
    GPUBufferBindingType type = "uniform";
    boolean hasDynamicOffset = false;
    GPUSize64 minBindingSize = 0;
};

enum GPUSamplerBindingType {
    "filtering",
    "non-filtering",
    "comparison",
};

dictionary GPUSamplerBindingLayout {
    GPUSamplerBindingType type = "filtering";
};

enum GPUTextureSampleType {
  "float",
  "unfilterable-float",
  "depth",
  "sint",
  "uint",
};

dictionary GPUTextureBindingLayout {
    GPUTextureSampleType sampleType = "float";
    GPUTextureViewDimension viewDimension = "2d";
    boolean multisampled = false;
};

enum GPUStorageTextureAccess {
    "read-only",
    "write-only",
};

dictionary GPUStorageTextureBindingLayout {
    required GPUStorageTextureAccess access;
    required GPUTextureFormat format;
    GPUTextureViewDimension viewDimension = "2d";
};

dictionary GPUBindGroupLayoutEntry {
    required GPUIndex32 binding;
    required GPUShaderStageFlags visibility;
    GPUBufferBindingLayout buffer;
    GPUSamplerBindingLayout sampler;
    GPUTextureBindingLayout texture;
    GPUStorageTextureBindingLayout storageTexture;
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

enum GPUCompilationMessageType {
    "error",
    "warning",
    "info"
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUCompilationMessage {
    readonly attribute DOMString message;
    readonly attribute GPUCompilationMessageType type;
    readonly attribute unsigned long long lineNum;
    readonly attribute unsigned long long linePos;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUCompilationInfo {
    //TODO:
    //[Cached, Frozen, Pure]
    //readonly attribute sequence<GPUCompilationMessage> messages;
};

// ShaderModule
//TODO: remove the `Uint32Array` variant, it's used for SPIR-V
typedef (Uint32Array or USVString) GPUShaderCode;

dictionary GPUShaderModuleDescriptor : GPUObjectDescriptorBase {
    required GPUShaderCode code;
    object sourceMap;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUShaderModule {
    //TODO:
    //Promise<GPUCompilationInfo> compilationInfo();
};
GPUShaderModule includes GPUObjectBase;


// Common stuff for ComputePipeline and RenderPipeline
dictionary GPUPipelineDescriptorBase : GPUObjectDescriptorBase {
    GPUPipelineLayout layout;
};

interface mixin GPUPipelineBase {
    GPUBindGroupLayout getBindGroupLayout(unsigned long index);
};

dictionary GPUProgrammableStage {
    required GPUShaderModule module;
    required USVString entryPoint;
};

// ComputePipeline
dictionary GPUComputePipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUProgrammableStage compute;
};

//TODO: Serializable
// https://bugzilla.mozilla.org/show_bug.cgi?id=1696219
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUComputePipeline {
};
GPUComputePipeline includes GPUObjectBase;
GPUComputePipeline includes GPUPipelineBase;

// InputState
enum GPUIndexFormat {
    "uint16",
    "uint32",
};

enum GPUVertexFormat {
    "uint8x2",
    "uint8x4",
    "sint8x2",
    "sint8x4",
    "unorm8x2",
    "unorm8x4",
    "snorm8x2",
    "snorm8x4",
    "uint16x2",
    "uint16x4",
    "sint16x2",
    "sint16x4",
    "unorm16x2",
    "unorm16x4",
    "snorm16x2",
    "snorm16x4",
    "float16x2",
    "float16x4",
    "float32",
    "float32x2",
    "float32x3",
    "float32x4",
    "uint32",
    "uint32x2",
    "uint32x3",
    "uint32x4",
    "sint32",
    "sint32x2",
    "sint32x3",
    "sint32x4",
};

enum GPUInputStepMode {
    "vertex",
    "instance",
};

dictionary GPUVertexAttribute {
    required GPUVertexFormat format;
    required GPUSize64 offset;
    required GPUIndex32 shaderLocation;
};

dictionary GPUVertexBufferLayout {
    required GPUSize64 arrayStride;
    GPUInputStepMode stepMode = "vertex";
    required sequence<GPUVertexAttribute> attributes;
};

dictionary GPUVertexState: GPUProgrammableStage {
    sequence<GPUVertexBufferLayout?> buffers = [];
};

// GPURenderPipeline
enum GPUPrimitiveTopology {
    "point-list",
    "line-list",
    "line-strip",
    "triangle-list",
    "triangle-strip"
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

dictionary GPUPrimitiveState {
    GPUPrimitiveTopology topology = "triangle-list";
    GPUIndexFormat stripIndexFormat;
    GPUFrontFace frontFace = "ccw";
    GPUCullMode cullMode = "none";
};

dictionary GPUMultisampleState {
    GPUSize32 count = 1;
    GPUSampleMask mask = 0xFFFFFFFF;
    boolean alphaToCoverageEnabled = false;
};

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

dictionary GPUBlendComponent {
    GPUBlendFactor srcFactor = "one";
    GPUBlendFactor dstFactor = "zero";
    GPUBlendOperation operation = "add";
};

dictionary GPUBlendState {
    required GPUBlendComponent color;
    required GPUBlendComponent alpha;
};

typedef [EnforceRange] unsigned long GPUColorWriteFlags;
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUColorWrite {
    const GPUColorWriteFlags RED    = 0x1;
    const GPUColorWriteFlags GREEN  = 0x2;
    const GPUColorWriteFlags BLUE   = 0x4;
    const GPUColorWriteFlags ALPHA  = 0x8;
    const GPUColorWriteFlags ALL    = 0xF;
};

dictionary GPUColorTargetState {
    required GPUTextureFormat format;
    GPUBlendState blend;
    GPUColorWriteFlags writeMask = 0xF;  // GPUColorWrite.ALL
};

dictionary GPUFragmentState: GPUProgrammableStage {
    required sequence<GPUColorTargetState> targets;
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

dictionary GPUStencilFaceState {
    GPUCompareFunction compare = "always";
    GPUStencilOperation failOp = "keep";
    GPUStencilOperation depthFailOp = "keep";
    GPUStencilOperation passOp = "keep";
};

dictionary GPUDepthStencilState {
    required GPUTextureFormat format;

    boolean depthWriteEnabled = false;
    GPUCompareFunction depthCompare = "always";

    GPUStencilFaceState stencilFront = {};
    GPUStencilFaceState stencilBack = {};

    GPUStencilValue stencilReadMask = 0xFFFFFFFF;
    GPUStencilValue stencilWriteMask = 0xFFFFFFFF;

    GPUDepthBias depthBias = 0;
    float depthBiasSlopeScale = 0;
    float depthBiasClamp = 0;

    // Enable depth clamping (requires "depth-clamping" feature)
    boolean clampDepth = false;
};

dictionary GPURenderPipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUVertexState vertex;
    GPUPrimitiveState primitive = {};
    GPUDepthStencilState depthStencil;
    GPUMultisampleState multisample = {};
    GPUFragmentState fragment;
};

//TODO: Serializable
// https://bugzilla.mozilla.org/show_bug.cgi?id=1696219
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderPipeline {
};
GPURenderPipeline includes GPUObjectBase;
GPURenderPipeline includes GPUPipelineBase;

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
    required GPUTextureView view;
    GPUTextureView resolveTarget;

    required (GPULoadOp or GPUColor) loadValue;
    GPUStoreOp storeOp = "store";
};

dictionary GPURenderPassDepthStencilAttachmentDescriptor {
    required GPUTextureView view;

    required (GPULoadOp or float) depthLoadValue;
    required GPUStoreOp depthStoreOp;

    required (GPULoadOp or GPUStencilValue) stencilLoadValue;
    required GPUStoreOp stencilStoreOp;
};

dictionary GPURenderPassDescriptor : GPUObjectDescriptorBase {
    required sequence<GPURenderPassColorAttachmentDescriptor> colorAttachments;
    GPURenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
    GPUQuerySet occlusionQuerySet;
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

    //void pushDebugGroup(USVString groupLabel);
    //void popDebugGroup();
    //void insertDebugMarker(USVString markerLabel);

    [NewObject]
    GPUCommandBuffer finish(optional GPUCommandBufferDescriptor descriptor = {});
};
GPUCommandEncoder includes GPUObjectBase;

interface mixin GPUProgrammablePassEncoder {
    void setBindGroup(GPUIndex32 index, GPUBindGroup bindGroup,
                      optional sequence<GPUBufferDynamicOffset> dynamicOffsets = []);

    //void pushDebugGroup(USVString groupLabel);
    //void popDebugGroup();
    //void insertDebugMarker(USVString markerLabel);
};

// Render Pass
interface mixin GPURenderEncoderBase {
    void setPipeline(GPURenderPipeline pipeline);

    void setIndexBuffer(GPUBuffer buffer, GPUIndexFormat indexFormat, optional GPUSize64 offset = 0, optional GPUSize64 size = 0);
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
    void setViewport(float x, float y,
                     float width, float height,
                     float minDepth, float maxDepth);

    void setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
                        GPUIntegerCoordinate width, GPUIntegerCoordinate height);

    void setBlendColor(GPUColor color);
    void setStencilReference(GPUStencilValue reference);

    //void beginOcclusionQuery(GPUSize32 queryIndex);
    //void endOcclusionQuery();

    //void beginPipelineStatisticsQuery(GPUQuerySet querySet, GPUSize32 queryIndex);
    //void endPipelineStatisticsQuery();

    //void writeTimestamp(GPUQuerySet querySet, GPUSize32 queryIndex);

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

// Render Bundle
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderBundle {
};
GPURenderBundle includes GPUObjectBase;

dictionary GPURenderBundleDescriptor : GPUObjectDescriptorBase {
};

dictionary GPURenderBundleEncoderDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUTextureFormat> colorFormats;
    GPUTextureFormat depthStencilFormat;
    GPUSize32 sampleCount = 1;
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPURenderBundleEncoder {
    //GPURenderBundle finish(optional GPURenderBundleDescriptor descriptor = {});
};
GPURenderBundleEncoder includes GPUObjectBase;
//TODO
//GPURenderBundleEncoder includes GPUProgrammablePassEncoder;
//GPURenderBundleEncoder includes GPURenderEncoderBase;

// ****************************************************************************
// OTHER (Query, Queue, SwapChain, Device)
// ****************************************************************************

// Query set
enum GPUQueryType {
    "occlusion",
    "pipeline-statistics",
    "timestamp"
};

enum GPUPipelineStatisticName {
    "vertex-shader-invocations",
    "clipper-invocations",
    "clipper-primitives-out",
    "fragment-shader-invocations",
    "compute-shader-invocations"
};

dictionary GPUQuerySetDescriptor : GPUObjectDescriptorBase {
    required GPUQueryType type;
    required GPUSize32 count;
    sequence<GPUPipelineStatisticName> pipelineStatistics = [];
};

[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUQuerySet {
    void destroy();
};
GPUQuerySet includes GPUObjectBase;

//TODO: use [AllowShared] on BufferSource
// https://bugzilla.mozilla.org/show_bug.cgi?id=1696216
// https://github.com/heycam/webidl/issues/961

// Queue
[Pref="dom.webgpu.enabled",
 Exposed=Window]
interface GPUQueue {
    void submit(sequence<GPUCommandBuffer> buffers);

    //TODO:
    //Promise<void> onSubmittedWorkDone();

    [Throws]
    void writeBuffer(
        GPUBuffer buffer,
        GPUSize64 bufferOffset,
        BufferSource data,
        optional GPUSize64 dataOffset = 0,
        optional GPUSize64 size);

    [Throws]
    void writeTexture(
      GPUTextureCopyView destination,
      BufferSource data,
      GPUTextureDataLayout dataLayout,
      GPUExtent3D size);
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

    GPUTextureFormat getSwapChainPreferredFormat(GPUAdapter adapter);
};
