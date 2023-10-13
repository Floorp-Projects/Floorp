// MAINTENANCE_TODO: The generated Typedoc for this file is hard to navigate because it's
// alphabetized. Consider using namespaces or renames to fix this?

/* eslint-disable no-sparse-arrays */

import { keysOf, makeTable, numericKeysOf, valueof } from '../common/util/data_tables.js';
import { assertTypeTrue, TypeEqual } from '../common/util/types.js';
import { unreachable } from '../common/util/util.js';

import { GPUConst, kMaxUnsignedLongValue, kMaxUnsignedLongLongValue } from './constants.js';

// Base device limits can be found in constants.ts.

// Queries

/** Maximum number of queries in GPUQuerySet, by spec. */
export const kMaxQueryCount = 4096;
/** Per-GPUQueryType info. */
export type QueryTypeInfo = {
  /** Optional feature required to use this GPUQueryType. */
  readonly feature: GPUFeatureName | undefined;
  // Add fields as needed
};
export const kQueryTypeInfo: {
  readonly [k in GPUQueryType]: QueryTypeInfo;
} = /* prettier-ignore */ {
  // Occlusion query does not require any features.
  'occlusion':           { feature:  undefined },
  'timestamp':           { feature: 'timestamp-query' },
};
/** List of all GPUQueryType values. */
export const kQueryTypes = keysOf(kQueryTypeInfo);

// Buffers

/** Required alignment of a GPUBuffer size, by spec. */
export const kBufferSizeAlignment = 4;

/** Per-GPUBufferUsage copy info. */
export const kBufferUsageCopyInfo: {
  readonly [name: string]: GPUBufferUsageFlags;
} = /* prettier-ignore */ {
  'COPY_NONE':    0,
  'COPY_SRC':     GPUConst.BufferUsage.COPY_SRC,
  'COPY_DST':     GPUConst.BufferUsage.COPY_DST,
  'COPY_SRC_DST': GPUConst.BufferUsage.COPY_SRC | GPUConst.BufferUsage.COPY_DST,
};
/** List of all GPUBufferUsage copy values. */
export const kBufferUsageCopy = keysOf(kBufferUsageCopyInfo);

/** Per-GPUBufferUsage keys and info. */
type BufferUsageKey = keyof typeof GPUConst.BufferUsage;
export const kBufferUsageKeys = keysOf(GPUConst.BufferUsage);
export const kBufferUsageInfo: {
  readonly [k in BufferUsageKey]: GPUBufferUsageFlags;
} = {
  ...GPUConst.BufferUsage,
};

/** List of all GPUBufferUsage values. */
export const kBufferUsages = Object.values(GPUConst.BufferUsage);
export const kAllBufferUsageBits = kBufferUsages.reduce(
  (previousSet, currentUsage) => previousSet | currentUsage,
  0
);

// Errors

/** Per-GPUErrorFilter info. */
export const kErrorScopeFilterInfo: {
  readonly [k in GPUErrorFilter]: {
    generatable: boolean;
  };
} = /* prettier-ignore */ {
  'internal':      { generatable: false },
  'out-of-memory': { generatable: true },
  'validation':    { generatable: true },
};
/** List of all GPUErrorFilter values. */
export const kErrorScopeFilters = keysOf(kErrorScopeFilterInfo);
export const kGeneratableErrorScopeFilters = kErrorScopeFilters.filter(
  e => kErrorScopeFilterInfo[e].generatable
);

// Canvases

// The formats of GPUTextureFormat for canvas context.
export const kCanvasTextureFormats = ['bgra8unorm', 'rgba8unorm', 'rgba16float'] as const;

// The alpha mode for canvas context.
export const kCanvasAlphaModesInfo: {
  readonly [k in GPUCanvasAlphaMode]: {};
} = /* prettier-ignore */ {
  'opaque': {},
  'premultiplied': {},
};
export const kCanvasAlphaModes = keysOf(kCanvasAlphaModesInfo);

// The color spaces for canvas context
export const kCanvasColorSpacesInfo: {
  readonly [k in PredefinedColorSpace]: {};
} = /* prettier-ignore */ {
  'srgb': {},
  'display-p3': {},
};
export const kCanvasColorSpaces = keysOf(kCanvasColorSpacesInfo);

// Textures (except for texture format info)

/** Per-GPUTextureDimension info. */
export const kTextureDimensionInfo: {
  readonly [k in GPUTextureDimension]: {};
} = /* prettier-ignore */ {
  '1d': {},
  '2d': {},
  '3d': {},
};
/** List of all GPUTextureDimension values. */
export const kTextureDimensions = keysOf(kTextureDimensionInfo);

/** Per-GPUTextureAspect info. */
export const kTextureAspectInfo: {
  readonly [k in GPUTextureAspect]: {};
} = /* prettier-ignore */ {
  'all': {},
  'depth-only': {},
  'stencil-only': {},
};
/** List of all GPUTextureAspect values. */
export const kTextureAspects = keysOf(kTextureAspectInfo);

// Misc

/** Per-GPUCompareFunction info. */
export const kCompareFunctionInfo: {
  readonly [k in GPUCompareFunction]: {};
} = /* prettier-ignore */ {
  'never': {},
  'less': {},
  'equal': {},
  'less-equal': {},
  'greater': {},
  'not-equal': {},
  'greater-equal': {},
  'always': {},
};
/** List of all GPUCompareFunction values. */
export const kCompareFunctions = keysOf(kCompareFunctionInfo);

/** Per-GPUStencilOperation info. */
export const kStencilOperationInfo: {
  readonly [k in GPUStencilOperation]: {};
} = /* prettier-ignore */ {
  'keep': {},
  'zero': {},
  'replace': {},
  'invert': {},
  'increment-clamp': {},
  'decrement-clamp': {},
  'increment-wrap': {},
  'decrement-wrap': {},
};
/** List of all GPUStencilOperation values. */
export const kStencilOperations = keysOf(kStencilOperationInfo);

// More textures (except for texture format info)

/** Per-GPUTextureUsage type info. */
export const kTextureUsageTypeInfo: {
  readonly [name: string]: number;
} = /* prettier-ignore */ {
  'texture': Number(GPUConst.TextureUsage.TEXTURE_BINDING),
  'storage': Number(GPUConst.TextureUsage.STORAGE_BINDING),
  'render':  Number(GPUConst.TextureUsage.RENDER_ATTACHMENT),
};
/** List of all GPUTextureUsage type values. */
export const kTextureUsageType = keysOf(kTextureUsageTypeInfo);

/** Per-GPUTextureUsage copy info. */
export const kTextureUsageCopyInfo: {
  readonly [name: string]: number;
} = /* prettier-ignore */ {
  'none':     0,
  'src':      Number(GPUConst.TextureUsage.COPY_SRC),
  'dst':      Number(GPUConst.TextureUsage.COPY_DST),
  'src-dest': Number(GPUConst.TextureUsage.COPY_SRC) | Number(GPUConst.TextureUsage.COPY_DST),
};
/** List of all GPUTextureUsage copy values. */
export const kTextureUsageCopy = keysOf(kTextureUsageCopyInfo);

/** Per-GPUTextureUsage info. */
export const kTextureUsageInfo: {
  readonly [k in valueof<typeof GPUConst.TextureUsage>]: {};
} = {
  [GPUConst.TextureUsage.COPY_SRC]: {},
  [GPUConst.TextureUsage.COPY_DST]: {},
  [GPUConst.TextureUsage.TEXTURE_BINDING]: {},
  [GPUConst.TextureUsage.STORAGE_BINDING]: {},
  [GPUConst.TextureUsage.RENDER_ATTACHMENT]: {},
};
/** List of all GPUTextureUsage values. */
export const kTextureUsages = numericKeysOf<GPUTextureUsageFlags>(kTextureUsageInfo);

// Texture View

/** Per-GPUTextureViewDimension info. */
export type TextureViewDimensionInfo = {
  /** Whether a storage texture view can have this view dimension. */
  readonly storage: boolean;
  // Add fields as needed
};
/** Per-GPUTextureViewDimension info. */
export const kTextureViewDimensionInfo: {
  readonly [k in GPUTextureViewDimension]: TextureViewDimensionInfo;
} = /* prettier-ignore */ {
  '1d':         { storage: true  },
  '2d':         { storage: true  },
  '2d-array':   { storage: true  },
  'cube':       { storage: false },
  'cube-array': { storage: false },
  '3d':         { storage: true  },
};
/** List of all GPUTextureDimension values. */
export const kTextureViewDimensions = keysOf(kTextureViewDimensionInfo);

// Vertex formats

/** Per-GPUVertexFormat info. */
// Exists just for documentation. Otherwise could be inferred by `makeTable`.
export type VertexFormatInfo = {
  /** Number of bytes in each component. */
  readonly bytesPerComponent: 1 | 2 | 4;
  /** The data encoding (float, normalized, or integer) for each component. */
  readonly type: 'float' | 'unorm' | 'snorm' | 'uint' | 'sint';
  /** Number of components. */
  readonly componentCount: 1 | 2 | 3 | 4;
  /** The completely matching WGSL type for vertex format */
  readonly wgslType:
    | 'f32'
    | 'vec2<f32>'
    | 'vec3<f32>'
    | 'vec4<f32>'
    | 'u32'
    | 'vec2<u32>'
    | 'vec3<u32>'
    | 'vec4<u32>'
    | 'i32'
    | 'vec2<i32>'
    | 'vec3<i32>'
    | 'vec4<i32>';
  // Add fields as needed
};
/** Per-GPUVertexFormat info. */
export const kVertexFormatInfo: {
  readonly [k in GPUVertexFormat]: VertexFormatInfo;
} = /* prettier-ignore */ makeTable(
               ['bytesPerComponent',  'type', 'componentCount',  'wgslType'] as const,
               [                   ,        ,                 ,            ] as const, {
  // 8 bit components
  'uint8x2':   [                  1,  'uint',                2, 'vec2<u32>'],
  'uint8x4':   [                  1,  'uint',                4, 'vec4<u32>'],
  'sint8x2':   [                  1,  'sint',                2, 'vec2<i32>'],
  'sint8x4':   [                  1,  'sint',                4, 'vec4<i32>'],
  'unorm8x2':  [                  1, 'unorm',                2, 'vec2<f32>'],
  'unorm8x4':  [                  1, 'unorm',                4, 'vec4<f32>'],
  'snorm8x2':  [                  1, 'snorm',                2, 'vec2<f32>'],
  'snorm8x4':  [                  1, 'snorm',                4, 'vec4<f32>'],
  // 16 bit components
  'uint16x2':  [                  2,  'uint',                2, 'vec2<u32>'],
  'uint16x4':  [                  2,  'uint',                4, 'vec4<u32>'],
  'sint16x2':  [                  2,  'sint',                2, 'vec2<i32>'],
  'sint16x4':  [                  2,  'sint',                4, 'vec4<i32>'],
  'unorm16x2': [                  2, 'unorm',                2, 'vec2<f32>'],
  'unorm16x4': [                  2, 'unorm',                4, 'vec4<f32>'],
  'snorm16x2': [                  2, 'snorm',                2, 'vec2<f32>'],
  'snorm16x4': [                  2, 'snorm',                4, 'vec4<f32>'],
  'float16x2': [                  2, 'float',                2, 'vec2<f32>'],
  'float16x4': [                  2, 'float',                4, 'vec4<f32>'],
  // 32 bit components
  'float32':   [                  4, 'float',                1,       'f32'],
  'float32x2': [                  4, 'float',                2, 'vec2<f32>'],
  'float32x3': [                  4, 'float',                3, 'vec3<f32>'],
  'float32x4': [                  4, 'float',                4, 'vec4<f32>'],
  'uint32':    [                  4,  'uint',                1,       'u32'],
  'uint32x2':  [                  4,  'uint',                2, 'vec2<u32>'],
  'uint32x3':  [                  4,  'uint',                3, 'vec3<u32>'],
  'uint32x4':  [                  4,  'uint',                4, 'vec4<u32>'],
  'sint32':    [                  4,  'sint',                1,       'i32'],
  'sint32x2':  [                  4,  'sint',                2, 'vec2<i32>'],
  'sint32x3':  [                  4,  'sint',                3, 'vec3<i32>'],
  'sint32x4':  [                  4,  'sint',                4, 'vec4<i32>']
} as const);
/** List of all GPUVertexFormat values. */
export const kVertexFormats = keysOf(kVertexFormatInfo);

// Typedefs for bindings

/**
 * Classes of `PerShaderStage` binding limits. Two bindings with the same class
 * count toward the same `PerShaderStage` limit(s) in the spec (if any).
 */
export type PerStageBindingLimitClass =
  | 'uniformBuf'
  | 'storageBuf'
  | 'sampler'
  | 'sampledTex'
  | 'storageTex';
/**
 * Classes of `PerPipelineLayout` binding limits. Two bindings with the same class
 * count toward the same `PerPipelineLayout` limit(s) in the spec (if any).
 */
export type PerPipelineBindingLimitClass = PerStageBindingLimitClass;

export type ValidBindableResource =
  | 'uniformBuf'
  | 'storageBuf'
  | 'filtSamp'
  | 'nonFiltSamp'
  | 'compareSamp'
  | 'sampledTex'
  | 'sampledTexMS'
  | 'storageTex';
type ErrorBindableResource = 'errorBuf' | 'errorSamp' | 'errorTex';

/**
 * Types of resource binding which have distinct binding rules, by spec
 * (e.g. filtering vs non-filtering sampler, multisample vs non-multisample texture).
 */
export type BindableResource = ValidBindableResource | ErrorBindableResource;
export const kBindableResources = [
  'uniformBuf',
  'storageBuf',
  'filtSamp',
  'nonFiltSamp',
  'compareSamp',
  'sampledTex',
  'sampledTexMS',
  'storageTex',
  'errorBuf',
  'errorSamp',
  'errorTex',
] as const;
assertTypeTrue<TypeEqual<BindableResource, typeof kBindableResources[number]>>();

// Bindings

/** Dynamic buffer offsets require offset to be divisible by 256, by spec. */
export const kMinDynamicBufferOffsetAlignment = 256;

/** Default `PerShaderStage` binding limits, by spec. */
export const kPerStageBindingLimits: {
  readonly [k in PerStageBindingLimitClass]: {
    /** Which `PerShaderStage` binding limit class. */
    readonly class: k;
    /** Maximum number of allowed bindings in that class. */
    readonly max: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', max: 12, },
  'storageBuf': { class: 'storageBuf', max:  8, },
  'sampler':    { class: 'sampler',    max: 16, },
  'sampledTex': { class: 'sampledTex', max: 16, },
  'storageTex': { class: 'storageTex', max:  4, },
};

/**
 * Default `PerPipelineLayout` binding limits, by spec.
 */
export const kPerPipelineBindingLimits: {
  readonly [k in PerPipelineBindingLimitClass]: {
    /** Which `PerPipelineLayout` binding limit class. */
    readonly class: k;
    /** Maximum number of allowed bindings with `hasDynamicOffset: true` in that class. */
    readonly maxDynamic: number;
    // Add fields as needed
  };
} = /* prettier-ignore */ {
  'uniformBuf': { class: 'uniformBuf', maxDynamic: 8, },
  'storageBuf': { class: 'storageBuf', maxDynamic: 4, },
  'sampler':    { class: 'sampler',    maxDynamic: 0, },
  'sampledTex': { class: 'sampledTex', maxDynamic: 0, },
  'storageTex': { class: 'storageTex', maxDynamic: 0, },
};

interface BindingKindInfo {
  readonly resource: ValidBindableResource;
  readonly perStageLimitClass: typeof kPerStageBindingLimits[PerStageBindingLimitClass];
  readonly perPipelineLimitClass: typeof kPerPipelineBindingLimits[PerPipelineBindingLimitClass];
  // Add fields as needed
}

const kBindingKind: {
  readonly [k in ValidBindableResource]: BindingKindInfo;
} = /* prettier-ignore */ {
  uniformBuf:   { resource: 'uniformBuf',   perStageLimitClass: kPerStageBindingLimits.uniformBuf, perPipelineLimitClass: kPerPipelineBindingLimits.uniformBuf, },
  storageBuf:   { resource: 'storageBuf',   perStageLimitClass: kPerStageBindingLimits.storageBuf, perPipelineLimitClass: kPerPipelineBindingLimits.storageBuf, },
  filtSamp:     { resource: 'filtSamp',     perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  nonFiltSamp:  { resource: 'nonFiltSamp',  perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  compareSamp:  { resource: 'compareSamp',  perStageLimitClass: kPerStageBindingLimits.sampler,    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,    },
  sampledTex:   { resource: 'sampledTex',   perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  sampledTexMS: { resource: 'sampledTexMS', perStageLimitClass: kPerStageBindingLimits.sampledTex, perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex, },
  storageTex:   { resource: 'storageTex',   perStageLimitClass: kPerStageBindingLimits.storageTex, perPipelineLimitClass: kPerPipelineBindingLimits.storageTex, },
};

// Binding type info

const kValidStagesAll = {
  validStages:
    GPUConst.ShaderStage.VERTEX | GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
} as const;
const kValidStagesStorageWrite = {
  validStages: GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
} as const;

/** Binding type info (including class limits) for the specified GPUBufferBindingLayout. */
export function bufferBindingTypeInfo(d: GPUBufferBindingLayout) {
  /* prettier-ignore */
  switch (d.type ?? 'uniform') {
    case 'uniform':           return { usage: GPUConst.BufferUsage.UNIFORM, ...kBindingKind.uniformBuf,  ...kValidStagesAll,          };
    case 'storage':           return { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesStorageWrite, };
    case 'read-only-storage': return { usage: GPUConst.BufferUsage.STORAGE, ...kBindingKind.storageBuf,  ...kValidStagesAll,          };
  }
}
/** List of all GPUBufferBindingType values. */
export const kBufferBindingTypes = ['uniform', 'storage', 'read-only-storage'] as const;
assertTypeTrue<TypeEqual<GPUBufferBindingType, typeof kBufferBindingTypes[number]>>();

/** Binding type info (including class limits) for the specified GPUSamplerBindingLayout. */
export function samplerBindingTypeInfo(d: GPUSamplerBindingLayout) {
  /* prettier-ignore */
  switch (d.type ?? 'filtering') {
    case 'filtering':     return { ...kBindingKind.filtSamp,    ...kValidStagesAll, };
    case 'non-filtering': return { ...kBindingKind.nonFiltSamp, ...kValidStagesAll, };
    case 'comparison':    return { ...kBindingKind.compareSamp, ...kValidStagesAll, };
  }
}
/** List of all GPUSamplerBindingType values. */
export const kSamplerBindingTypes = ['filtering', 'non-filtering', 'comparison'] as const;
assertTypeTrue<TypeEqual<GPUSamplerBindingType, typeof kSamplerBindingTypes[number]>>();

/** Binding type info (including class limits) for the specified GPUTextureBindingLayout. */
export function sampledTextureBindingTypeInfo(d: GPUTextureBindingLayout) {
  /* prettier-ignore */
  if (d.multisampled) {
    return { usage: GPUConst.TextureUsage.TEXTURE_BINDING, ...kBindingKind.sampledTexMS, ...kValidStagesAll, };
  } else {
    return { usage: GPUConst.TextureUsage.TEXTURE_BINDING, ...kBindingKind.sampledTex,   ...kValidStagesAll, };
  }
}
/** List of all GPUTextureSampleType values. */
export const kTextureSampleTypes = [
  'float',
  'unfilterable-float',
  'depth',
  'sint',
  'uint',
] as const;
assertTypeTrue<TypeEqual<GPUTextureSampleType, typeof kTextureSampleTypes[number]>>();

/** Binding type info (including class limits) for the specified GPUStorageTextureBindingLayout. */
export function storageTextureBindingTypeInfo(d: GPUStorageTextureBindingLayout) {
  return {
    usage: GPUConst.TextureUsage.STORAGE_BINDING,
    ...kBindingKind.storageTex,
    ...kValidStagesStorageWrite,
  };
}
/** List of all GPUStorageTextureAccess values. */
export const kStorageTextureAccessValues = ['write-only'] as const;
assertTypeTrue<TypeEqual<GPUStorageTextureAccess, typeof kStorageTextureAccessValues[number]>>();

/** GPUBindGroupLayoutEntry, but only the "union" fields, not the common fields. */
export type BGLEntry = Omit<GPUBindGroupLayoutEntry, 'binding' | 'visibility'>;
/** Binding type info (including class limits) for the specified BGLEntry. */
export function texBindingTypeInfo(e: BGLEntry) {
  if (e.texture !== undefined) return sampledTextureBindingTypeInfo(e.texture);
  if (e.storageTexture !== undefined) return storageTextureBindingTypeInfo(e.storageTexture);
  unreachable();
}
/** BindingTypeInfo (including class limits) for the specified BGLEntry. */
export function bindingTypeInfo(e: BGLEntry) {
  if (e.buffer !== undefined) return bufferBindingTypeInfo(e.buffer);
  if (e.texture !== undefined) return sampledTextureBindingTypeInfo(e.texture);
  if (e.sampler !== undefined) return samplerBindingTypeInfo(e.sampler);
  if (e.storageTexture !== undefined) return storageTextureBindingTypeInfo(e.storageTexture);
  unreachable('GPUBindGroupLayoutEntry has no BindingLayout');
}

/**
 * Generate a list of possible buffer-typed BGLEntry values.
 *
 * Note: Generates different `type` options, but not `hasDynamicOffset` options.
 */
export function bufferBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ buffer: { type: undefined } }] : []),
    { buffer: { type: 'uniform' } },
    { buffer: { type: 'storage' } },
    { buffer: { type: 'read-only-storage' } },
  ] as const;
}
/** Generate a list of possible sampler-typed BGLEntry values. */
export function samplerBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ sampler: { type: undefined } }] : []),
    { sampler: { type: 'comparison' } },
    { sampler: { type: 'filtering' } },
    { sampler: { type: 'non-filtering' } },
  ] as const;
}
/**
 * Generate a list of possible texture-typed BGLEntry values.
 *
 * Note: Generates different `multisampled` options, but not `sampleType` or `viewDimension` options.
 */
export function textureBindingEntries(includeUndefined: boolean): readonly BGLEntry[] {
  return [
    ...(includeUndefined ? [{ texture: { multisampled: undefined } }] : []),
    { texture: { multisampled: false } },
    { texture: { multisampled: true, sampleType: 'unfilterable-float' } },
  ] as const;
}
/**
 * Generate a list of possible storageTexture-typed BGLEntry values.
 *
 * Note: Generates different `access` options, but not `format` or `viewDimension` options.
 */
export function storageTextureBindingEntries(format: GPUTextureFormat): readonly BGLEntry[] {
  return [{ storageTexture: { access: 'write-only', format } }] as const;
}
/** Generate a list of possible texture-or-storageTexture-typed BGLEntry values. */
export function sampledAndStorageBindingEntries(
  includeUndefined: boolean,
  storageTextureFormat: GPUTextureFormat = 'rgba8unorm'
): readonly BGLEntry[] {
  return [
    ...textureBindingEntries(includeUndefined),
    ...storageTextureBindingEntries(storageTextureFormat),
  ] as const;
}
/**
 * Generate a list of possible BGLEntry values of every type, but not variants with different:
 * - buffer.hasDynamicOffset
 * - texture.sampleType
 * - texture.viewDimension
 * - storageTexture.viewDimension
 */
export function allBindingEntries(
  includeUndefined: boolean,
  storageTextureFormat: GPUTextureFormat = 'rgba8unorm'
): readonly BGLEntry[] {
  return [
    ...bufferBindingEntries(includeUndefined),
    ...samplerBindingEntries(includeUndefined),
    ...sampledAndStorageBindingEntries(includeUndefined, storageTextureFormat),
  ] as const;
}

// Shader stages

/** List of all GPUShaderStage values. */
export type ShaderStageKey = keyof typeof GPUConst.ShaderStage;
export const kShaderStageKeys = Object.keys(GPUConst.ShaderStage) as ShaderStageKey[];
export const kShaderStages: readonly GPUShaderStageFlags[] = [
  GPUConst.ShaderStage.VERTEX,
  GPUConst.ShaderStage.FRAGMENT,
  GPUConst.ShaderStage.COMPUTE,
];
/** List of all possible combinations of GPUShaderStage values. */
export const kShaderStageCombinations: readonly GPUShaderStageFlags[] = [0, 1, 2, 3, 4, 5, 6, 7];
export const kShaderStageCombinationsWithStage: readonly GPUShaderStageFlags[] = [
  1,
  2,
  3,
  4,
  5,
  6,
  7,
];

/**
 * List of all possible texture sampleCount values.
 *
 * MAINTENANCE_TODO: Switch existing tests to use kTextureSampleCounts
 */
export const kTextureSampleCounts = [1, 4] as const;

// Sampler info

/** List of all mipmap filter modes. */
export const kMipmapFilterModes: readonly GPUMipmapFilterMode[] = ['nearest', 'linear'];
assertTypeTrue<TypeEqual<GPUMipmapFilterMode, typeof kMipmapFilterModes[number]>>();

/** List of address modes. */
export const kAddressModes: readonly GPUAddressMode[] = [
  'clamp-to-edge',
  'repeat',
  'mirror-repeat',
];
assertTypeTrue<TypeEqual<GPUAddressMode, typeof kAddressModes[number]>>();

// Blend factors and Blend components

/** List of all GPUBlendFactor values. */
export const kBlendFactors: readonly GPUBlendFactor[] = [
  'zero',
  'one',
  'src',
  'one-minus-src',
  'src-alpha',
  'one-minus-src-alpha',
  'dst',
  'one-minus-dst',
  'dst-alpha',
  'one-minus-dst-alpha',
  'src-alpha-saturated',
  'constant',
  'one-minus-constant',
];

/** List of all GPUBlendOperation values. */
export const kBlendOperations: readonly GPUBlendOperation[] = [
  'add', //
  'subtract',
  'reverse-subtract',
  'min',
  'max',
];

// Primitive topologies
export const kPrimitiveTopology: readonly GPUPrimitiveTopology[] = [
  'point-list',
  'line-list',
  'line-strip',
  'triangle-list',
  'triangle-strip',
];
assertTypeTrue<TypeEqual<GPUPrimitiveTopology, typeof kPrimitiveTopology[number]>>();

export const kIndexFormat: readonly GPUIndexFormat[] = ['uint16', 'uint32'];
assertTypeTrue<TypeEqual<GPUIndexFormat, typeof kIndexFormat[number]>>();

/** Info for each entry of GPUSupportedLimits */
export const kLimitInfo = /* prettier-ignore */ makeTable(
                                               [    'class', 'default',            'maximumValue'] as const,
                                               [  'maximum',          ,     kMaxUnsignedLongValue] as const, {
  'maxTextureDimension1D':                     [           ,      8192,                          ],
  'maxTextureDimension2D':                     [           ,      8192,                          ],
  'maxTextureDimension3D':                     [           ,      2048,                          ],
  'maxTextureArrayLayers':                     [           ,       256,                          ],

  'maxBindGroups':                             [           ,         4,                          ],
  'maxBindingsPerBindGroup':                   [           ,      1000,                          ],
  'maxDynamicUniformBuffersPerPipelineLayout': [           ,         8,                          ],
  'maxDynamicStorageBuffersPerPipelineLayout': [           ,         4,                          ],
  'maxSampledTexturesPerShaderStage':          [           ,        16,                          ],
  'maxSamplersPerShaderStage':                 [           ,        16,                          ],
  'maxStorageBuffersPerShaderStage':           [           ,         8,                          ],
  'maxStorageTexturesPerShaderStage':          [           ,         4,                          ],
  'maxUniformBuffersPerShaderStage':           [           ,        12,                          ],

  'maxUniformBufferBindingSize':               [           ,     65536, kMaxUnsignedLongLongValue],
  'maxStorageBufferBindingSize':               [           , 134217728, kMaxUnsignedLongLongValue],
  'minUniformBufferOffsetAlignment':           ['alignment',       256,                          ],
  'minStorageBufferOffsetAlignment':           ['alignment',       256,                          ],

  'maxVertexBuffers':                          [           ,         8,                          ],
  'maxBufferSize':                             [           , 268435456, kMaxUnsignedLongLongValue],
  'maxVertexAttributes':                       [           ,        16,                          ],
  'maxVertexBufferArrayStride':                [           ,      2048,                          ],
  'maxInterStageShaderComponents':             [           ,        60,                          ],
  'maxInterStageShaderVariables':              [           ,        16,                          ],

  'maxColorAttachments':                       [           ,         8,                          ],
  'maxColorAttachmentBytesPerSample':          [           ,        32,                          ],

  'maxComputeWorkgroupStorageSize':            [           ,     16384,                          ],
  'maxComputeInvocationsPerWorkgroup':         [           ,       256,                          ],
  'maxComputeWorkgroupSizeX':                  [           ,       256,                          ],
  'maxComputeWorkgroupSizeY':                  [           ,       256,                          ],
  'maxComputeWorkgroupSizeZ':                  [           ,        64,                          ],
  'maxComputeWorkgroupsPerDimension':          [           ,     65535,                          ],
} as const);

/** List of all entries of GPUSupportedLimits. */
export const kLimits = keysOf(kLimitInfo);

// Pipeline limits

/** Maximum number of color attachments to a render pass, by spec. */
export const kMaxColorAttachments = kLimitInfo.maxColorAttachments.default;
/** `maxVertexBuffers` per GPURenderPipeline, by spec. */
export const kMaxVertexBuffers = kLimitInfo.maxVertexBuffers.default;
/** `maxVertexAttributes` per GPURenderPipeline, by spec. */
export const kMaxVertexAttributes = kLimitInfo.maxVertexAttributes.default;
/** `maxVertexBufferArrayStride` in a vertex buffer in a GPURenderPipeline, by spec. */
export const kMaxVertexBufferArrayStride = kLimitInfo.maxVertexBufferArrayStride.default;

/** The size of indirect draw parameters in the indirectBuffer of drawIndirect */
export const kDrawIndirectParametersSize = 4;
/** The size of indirect drawIndexed parameters in the indirectBuffer of drawIndexedIndirect */
export const kDrawIndexedIndirectParametersSize = 5;

/** Per-GPUFeatureName info. */
export const kFeatureNameInfo: {
  readonly [k in GPUFeatureName]: {};
} = /* prettier-ignore */ {
  'bgra8unorm-storage': {},
  'depth-clip-control': {},
  'depth32float-stencil8': {},
  'texture-compression-bc': {},
  'texture-compression-etc2': {},
  'texture-compression-astc': {},
  'timestamp-query': {},
  'indirect-first-instance': {},
  'shader-f16': {},
  'rg11b10ufloat-renderable': {},
  'float32-filterable': {},
};
/** List of all GPUFeatureName values. */
export const kFeatureNames = keysOf(kFeatureNameInfo);
