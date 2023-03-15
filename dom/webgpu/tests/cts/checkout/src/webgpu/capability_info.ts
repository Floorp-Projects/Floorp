// MAINTENANCE_TODO: The generated Typedoc for this file is hard to navigate because it's
// alphabetized. Consider using namespaces or renames to fix this?

/* eslint-disable no-sparse-arrays */

import { keysOf, makeTable, numericKeysOf, valueof } from '../common/util/data_tables.js';
import { assertTypeTrue, TypeEqual } from '../common/util/types.js';
import { assert, unreachable } from '../common/util/util.js';

import { GPUConst, kMaxUnsignedLongValue, kMaxUnsignedLongLongValue } from './constants.js';
import { ImageCopyType } from './util/texture/layout.js';

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
  readonly [k in GPUErrorFilter]: {};
} = /* prettier-ignore */ {
  'out-of-memory': {},
  'validation':    {},
  'internal':      {},
};
/** List of all GPUErrorFilter values. */
export const kErrorScopeFilters = keysOf(kErrorScopeFilterInfo);
export const kGeneratableErrorScopeFilters = kErrorScopeFilters.filter(e => e !== 'internal');

// Textures

// Definitions for use locally. To access the table entries, use `kTextureFormatInfo`.

// Note that we repeat the header multiple times in order to make it easier to read.
const kRegularTextureFormatInfo = /* prettier-ignore */ makeTable(
                           ['renderable', 'multisample', 'resolve', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst',         'sampleType', 'bytesPerBlock', 'blockWidth', 'blockHeight', 'renderTargetPixelByteCost', 'renderTargetComponentAlignment',                  'feature',       'baseFormat'] as const,
                           [            ,              ,          ,    true,   false,     false,          ,      true,      true,                     ,                ,            1,             1,                   undefined,                        undefined,                           ,          undefined] as const, {
  // 8-bit formats
  'r8unorm':               [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               1,             ,              ,                           1,                                1],
  'r8snorm':               [       false,         false,     false,        ,        ,          ,     false,          ,          ,              'float',               1],
  'r8uint':                [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'uint',               1,             ,              ,                           1,                                1],
  'r8sint':                [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'sint',               1,             ,              ,                           1,                                1],
  // 16-bit formats
  'r16uint':               [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'uint',               2,             ,              ,                           2,                                2],
  'r16sint':               [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'sint',               2,             ,              ,                           2,                                2],
  'r16float':              [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               2,             ,              ,                           2,                                2],
  'rg8unorm':              [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               2,             ,              ,                           2,                                1],
  'rg8snorm':              [       false,         false,     false,        ,        ,          ,     false,          ,          ,              'float',               2],
  'rg8uint':               [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'uint',               2,             ,              ,                           2,                                1],
  'rg8sint':               [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'sint',               2,             ,              ,                           2,                                1],
  // 32-bit formats
  'r32uint':               [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'uint',               4,             ,              ,                           4,                                4],
  'r32sint':               [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'sint',               4,             ,              ,                           4,                                4],
  'r32float':              [        true,          true,     false,        ,        ,          ,      true,          ,          , 'unfilterable-float',               4,             ,              ,                           4,                                4],
  'rg16uint':              [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'uint',               4,             ,              ,                           4,                                2],
  'rg16sint':              [        true,          true,     false,        ,        ,          ,     false,          ,          ,               'sint',               4,             ,              ,                           4,                                2],
  'rg16float':             [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           4,                                2],
  'rgba8unorm':            [        true,          true,      true,        ,        ,          ,      true,          ,          ,              'float',               4,             ,              ,                           8,                                1,                           ,       'rgba8unorm'],
  'rgba8unorm-srgb':       [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           8,                                1,                           ,       'rgba8unorm'],
  'rgba8snorm':            [       false,         false,     false,        ,        ,          ,      true,          ,          ,              'float',               4],
  'rgba8uint':             [        true,          true,     false,        ,        ,          ,      true,          ,          ,               'uint',               4,             ,              ,                           4,                                1],
  'rgba8sint':             [        true,          true,     false,        ,        ,          ,      true,          ,          ,               'sint',               4,             ,              ,                           4,                                1],
  'bgra8unorm':            [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           8,                                1,                           ,       'bgra8unorm'],
  'bgra8unorm-srgb':       [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           8,                                1,                           ,       'bgra8unorm'],
  // Packed 32-bit formats
  'rgb10a2unorm':          [        true,          true,      true,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           8,                                4],
  'rg11b10ufloat':         [       false,         false,     false,        ,        ,          ,     false,          ,          ,              'float',               4,             ,              ,                           8,                                4],
  'rgb9e5ufloat':          [       false,         false,     false,        ,        ,          ,     false,          ,          ,              'float',               4],
  // 64-bit formats
  'rg32uint':              [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'uint',               8,             ,              ,                           8,                                4],
  'rg32sint':              [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'sint',               8,             ,              ,                           8,                                4],
  'rg32float':             [        true,         false,     false,        ,        ,          ,      true,          ,          , 'unfilterable-float',               8,             ,              ,                           8,                                4],
  'rgba16uint':            [        true,          true,     false,        ,        ,          ,      true,          ,          ,               'uint',               8,             ,              ,                           8,                                2],
  'rgba16sint':            [        true,          true,     false,        ,        ,          ,      true,          ,          ,               'sint',               8,             ,              ,                           8,                                2],
  'rgba16float':           [        true,          true,      true,        ,        ,          ,      true,          ,          ,              'float',               8,             ,              ,                           8,                                2],
  // 128-bit formats
  'rgba32uint':            [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'uint',              16,             ,              ,                          16,                                4],
  'rgba32sint':            [        true,         false,     false,        ,        ,          ,      true,          ,          ,               'sint',              16,             ,              ,                          16,                                4],
  'rgba32float':           [        true,         false,     false,        ,        ,          ,      true,          ,          , 'unfilterable-float',              16,             ,              ,                          16,                                4],
} as const);
/* prettier-ignore */
const kTexFmtInfoHeader =  ['renderable', 'multisample', 'resolve', 'color', 'depth', 'stencil', 'storage', 'copySrc', 'copyDst',         'sampleType', 'bytesPerBlock', 'blockWidth', 'blockHeight', 'renderTargetPixelByteCost', 'renderTargetComponentAlignment',                  'feature',       'baseFormat'] as const;
const kSizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [        true,          true,     false,   false,        ,          ,     false,          ,          ,                     ,                ,            1,             1,                   undefined,                        undefined,                           ,          undefined] as const, {
  'depth32float':          [            ,              ,          ,        ,    true,     false,          ,      true,     false,              'depth',               4],
  'depth16unorm':          [            ,              ,          ,        ,    true,     false,          ,      true,      true,              'depth',               2],
  'stencil8':              [            ,              ,          ,        ,   false,      true,          ,      true,      true,               'uint',               1],
} as const);

// Multi aspect sample type are now set to their first aspect
const kUnsizedDepthStencilFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [        true,          true,     false,   false,        ,          ,     false,     false,     false,                     ,       undefined,            1,             1,                            ,                                 ,                           ,          undefined] as const, {
  'depth24plus':           [            ,              ,          ,        ,    true,     false,          ,          ,          ,              'depth'],
  'depth24plus-stencil8':  [            ,              ,          ,        ,    true,      true,          ,          ,          ,              'depth'],
  // MAINTENANCE_TODO: These should really be sized formats; see below MAINTENANCE_TODO about multi-aspect formats.
  'depth32float-stencil8': [            ,              ,          ,        ,    true,      true,          ,          ,          ,              'depth',                ,             ,              ,                            ,                                 ,    'depth32float-stencil8'],
} as const);

// Separated compressed formats by type
const kBCTextureFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [       false,         false,     false,    true,   false,     false,     false,      true,      true,                     ,                ,            4,             4,                            ,                                 ,                           ,          undefined] as const, {
  // Block Compression (BC) formats
  'bc1-rgba-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc1-rgba-unorm'],
  'bc1-rgba-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc1-rgba-unorm'],
  'bc2-rgba-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc2-rgba-unorm'],
  'bc2-rgba-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc2-rgba-unorm'],
  'bc3-rgba-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc3-rgba-unorm'],
  'bc3-rgba-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc3-rgba-unorm'],
  'bc4-r-unorm':           [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc4-r-snorm':           [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc5-rg-unorm':          [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc5-rg-snorm':          [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc6h-rgb-ufloat':       [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc6h-rgb-float':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc'],
  'bc7-rgba-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc7-rgba-unorm'],
  'bc7-rgba-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 ,   'texture-compression-bc',   'bc7-rgba-unorm'],
} as const);
const kETC2TextureFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [       false,         false,     false,    true,   false,     false,     false,      true,      true,                     ,                ,            4,             4,                            ,                                 ,                           ,          undefined] as const, {
  // Ericsson Compression (ETC2) formats
  'etc2-rgb8unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2',   'etc2-rgb8unorm'],
  'etc2-rgb8unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2',   'etc2-rgb8unorm'],
  'etc2-rgb8a1unorm':      [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2', 'etc2-rgb8a1unorm'],
  'etc2-rgb8a1unorm-srgb': [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2', 'etc2-rgb8a1unorm'],
  'etc2-rgba8unorm':       [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-etc2',  'etc2-rgba8unorm'],
  'etc2-rgba8unorm-srgb':  [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-etc2',  'etc2-rgba8unorm'],
  'eac-r11unorm':          [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2'],
  'eac-r11snorm':          [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',               8,            4,             4,                            ,                                 , 'texture-compression-etc2'],
  'eac-rg11unorm':         [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-etc2'],
  'eac-rg11snorm':         [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-etc2'],
} as const);
const kASTCTextureFormatInfo = /* prettier-ignore */ makeTable(kTexFmtInfoHeader,
                           [       false,         false,     false,    true,   false,     false,     false,      true,      true,                     ,                ,             ,              ,                            ,                                 ,                           ,          undefined] as const, {
  // Adaptable Scalable Compression (ASTC) formats
  'astc-4x4-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-astc',   'astc-4x4-unorm'],
  'astc-4x4-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            4,             4,                            ,                                 , 'texture-compression-astc',   'astc-4x4-unorm'],
  'astc-5x4-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            5,             4,                            ,                                 , 'texture-compression-astc',   'astc-5x4-unorm'],
  'astc-5x4-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            5,             4,                            ,                                 , 'texture-compression-astc',   'astc-5x4-unorm'],
  'astc-5x5-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            5,             5,                            ,                                 , 'texture-compression-astc',   'astc-5x5-unorm'],
  'astc-5x5-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            5,             5,                            ,                                 , 'texture-compression-astc',   'astc-5x5-unorm'],
  'astc-6x5-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            6,             5,                            ,                                 , 'texture-compression-astc',   'astc-6x5-unorm'],
  'astc-6x5-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            6,             5,                            ,                                 , 'texture-compression-astc',   'astc-6x5-unorm'],
  'astc-6x6-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            6,             6,                            ,                                 , 'texture-compression-astc',   'astc-6x6-unorm'],
  'astc-6x6-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            6,             6,                            ,                                 , 'texture-compression-astc',   'astc-6x6-unorm'],
  'astc-8x5-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             5,                            ,                                 , 'texture-compression-astc',   'astc-8x5-unorm'],
  'astc-8x5-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             5,                            ,                                 , 'texture-compression-astc',   'astc-8x5-unorm'],
  'astc-8x6-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             6,                            ,                                 , 'texture-compression-astc',   'astc-8x6-unorm'],
  'astc-8x6-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             6,                            ,                                 , 'texture-compression-astc',   'astc-8x6-unorm'],
  'astc-8x8-unorm':        [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             8,                            ,                                 , 'texture-compression-astc',   'astc-8x8-unorm'],
  'astc-8x8-unorm-srgb':   [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,            8,             8,                            ,                                 , 'texture-compression-astc',   'astc-8x8-unorm'],
  'astc-10x5-unorm':       [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             5,                            ,                                 , 'texture-compression-astc',  'astc-10x5-unorm'],
  'astc-10x5-unorm-srgb':  [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             5,                            ,                                 , 'texture-compression-astc',  'astc-10x5-unorm'],
  'astc-10x6-unorm':       [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             6,                            ,                                 , 'texture-compression-astc',  'astc-10x6-unorm'],
  'astc-10x6-unorm-srgb':  [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             6,                            ,                                 , 'texture-compression-astc',  'astc-10x6-unorm'],
  'astc-10x8-unorm':       [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             8,                            ,                                 , 'texture-compression-astc',  'astc-10x8-unorm'],
  'astc-10x8-unorm-srgb':  [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,             8,                            ,                                 , 'texture-compression-astc',  'astc-10x8-unorm'],
  'astc-10x10-unorm':      [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,            10,                            ,                                 , 'texture-compression-astc', 'astc-10x10-unorm'],
  'astc-10x10-unorm-srgb': [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           10,            10,                            ,                                 , 'texture-compression-astc', 'astc-10x10-unorm'],
  'astc-12x10-unorm':      [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           12,            10,                            ,                                 , 'texture-compression-astc', 'astc-12x10-unorm'],
  'astc-12x10-unorm-srgb': [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           12,            10,                            ,                                 , 'texture-compression-astc', 'astc-12x10-unorm'],
  'astc-12x12-unorm':      [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           12,            12,                            ,                                 , 'texture-compression-astc', 'astc-12x12-unorm'],
  'astc-12x12-unorm-srgb': [            ,              ,          ,        ,        ,          ,          ,          ,          ,              'float',              16,           12,            12,                            ,                                 , 'texture-compression-astc', 'astc-12x12-unorm'],
} as const);

// Definitions for use locally. To access the table entries, use `kTextureFormatInfo`.

// MAINTENANCE_TODO: Consider generating the exports below programmatically by filtering the big list, instead
// of using these local constants? Requires some type magic though.
/* prettier-ignore */ const   kCompressedTextureFormatInfo = { ...kBCTextureFormatInfo, ...kETC2TextureFormatInfo, ...kASTCTextureFormatInfo } as const;
/* prettier-ignore */ const        kColorTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kCompressedTextureFormatInfo } as const;
/* prettier-ignore */ const    kEncodableTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const        kSizedTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo, ...kCompressedTextureFormatInfo } as const;
/* prettier-ignore */ const        kDepthStencilFormatInfo = { ...kSizedDepthStencilFormatInfo, ...kUnsizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const kUncompressedTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo, ...kUnsizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const          kAllTextureFormatInfo = { ...kUncompressedTextureFormatInfo, ...kCompressedTextureFormatInfo } as const;

/** A "regular" texture format (uncompressed, sized, single-plane color formats). */
/* prettier-ignore */ export type      RegularTextureFormat = keyof typeof kRegularTextureFormatInfo;
/** A sized depth/stencil texture format. */
/* prettier-ignore */ export type   SizedDepthStencilFormat = keyof typeof kSizedDepthStencilFormatInfo;
/** An unsized depth/stencil texture format. */
/* prettier-ignore */ export type UnsizedDepthStencilFormat = keyof typeof kUnsizedDepthStencilFormatInfo;
/** A compressed (block) texture format. */
/* prettier-ignore */ export type   CompressedTextureFormat = keyof typeof kCompressedTextureFormatInfo;

/** A color texture format (regular | compressed). */
/* prettier-ignore */ export type        ColorTextureFormat = keyof typeof kColorTextureFormatInfo;
/** An encodable texture format (regular | sized depth/stencil). */
/* prettier-ignore */ export type    EncodableTextureFormat = keyof typeof kEncodableTextureFormatInfo;
/** A sized texture format (regular | sized depth/stencil | compressed). */
/* prettier-ignore */ export type        SizedTextureFormat = keyof typeof kSizedTextureFormatInfo;
/** A depth/stencil format (sized | unsized). */
/* prettier-ignore */ export type        DepthStencilFormat = keyof typeof kDepthStencilFormatInfo;
/** An uncompressed (block size 1x1) format (regular | depth/stencil). */
/* prettier-ignore */ export type UncompressedTextureFormat = keyof typeof kUncompressedTextureFormatInfo;

/* prettier-ignore */ export const      kRegularTextureFormats: readonly      RegularTextureFormat[] = keysOf(     kRegularTextureFormatInfo);
/* prettier-ignore */ export const   kSizedDepthStencilFormats: readonly   SizedDepthStencilFormat[] = keysOf(  kSizedDepthStencilFormatInfo);
/* prettier-ignore */ export const kUnsizedDepthStencilFormats: readonly UnsizedDepthStencilFormat[] = keysOf(kUnsizedDepthStencilFormatInfo);
/* prettier-ignore */ export const   kCompressedTextureFormats: readonly   CompressedTextureFormat[] = keysOf(  kCompressedTextureFormatInfo);

/* prettier-ignore */ export const        kColorTextureFormats: readonly        ColorTextureFormat[] = keysOf(       kColorTextureFormatInfo);
/* prettier-ignore */ export const    kEncodableTextureFormats: readonly    EncodableTextureFormat[] = keysOf(   kEncodableTextureFormatInfo);
/* prettier-ignore */ export const        kSizedTextureFormats: readonly        SizedTextureFormat[] = keysOf(       kSizedTextureFormatInfo);
/* prettier-ignore */ export const        kDepthStencilFormats: readonly        DepthStencilFormat[] = keysOf(       kDepthStencilFormatInfo);
/* prettier-ignore */ export const kUncompressedTextureFormats: readonly UncompressedTextureFormat[] = keysOf(kUncompressedTextureFormatInfo);
/* prettier-ignore */ export const          kAllTextureFormats: readonly          GPUTextureFormat[] = keysOf(         kAllTextureFormatInfo);

// CompressedTextureFormat are unrenderable so filter from RegularTextureFormats for color targets is enough
export const kRenderableColorTextureFormats = kRegularTextureFormats.filter(
  v => kColorTextureFormatInfo[v].renderable
);
assert(
  kRenderableColorTextureFormats.every(
    f =>
      kAllTextureFormatInfo[f].renderTargetComponentAlignment !== undefined &&
      kAllTextureFormatInfo[f].renderTargetPixelByteCost !== undefined
  )
);

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

/** Per-GPUTextureFormat info. */
// Exists just for documentation. Otherwise could be inferred by `makeTable`.
// MAINTENANCE_TODO: Refactor this to separate per-aspect data for multi-aspect formats. In particular:
// - bytesPerBlock only makes sense on a per-aspect basis. But this table can't express that.
//   So we put depth32float-stencil8 to be an unsized format for now.
export type TextureFormatInfo = {
  /** Whether the format can be used as `RENDER_ATTACHMENT`. */
  renderable: boolean;
  /** Whether the format can be used in a multisample texture. */
  multisample: boolean;
  /** Whether the texture with the format can be used as a resolve target. */
  resolve: boolean;
  /** Whether the format has a color aspect. */
  color: boolean;
  /** Whether the format has a depth aspect. */
  depth: boolean;
  /** Whether the format has a stencil aspect. */
  stencil: boolean;
  /** Whether the format can be used as `STORAGE`. */
  storage: boolean;
  /** Whether the format can be used as `COPY_SRC`. */
  copySrc: boolean;
  /** Whether the format can be used as `COPY_DST`. */
  copyDst: boolean;
  /** Byte size of one texel block, or `undefined` if the format is unsized. */
  bytesPerBlock: number | undefined;
  /** Width, in texels, of one texel block. */
  blockWidth: number;
  /** Height, in texels, of one texel block. */
  blockHeight: number;
  /** The raw, unaligned, byte cost towards the color attachment bytes per sample.
   *  (See https://www.w3.org/TR/webgpu/#abstract-opdef-calculating-color-attachment-bytes-per-sample). */
  renderTargetPixelByteCost: number | undefined;
  /** The alignment used for the format when computing the color attachment bytes per sample. */
  renderTargetComponentAlignment: number | undefined;
  /** Optional feature required to use this format, or `undefined` if none. */
  feature: GPUFeatureName | undefined;
  // Add fields as needed
};
/** Per-GPUTextureFormat info. */
export const kTextureFormatInfo: {
  readonly [k in GPUTextureFormat]: TextureFormatInfo &
    // TextureFormatInfo exists just for documentation (and verification of the table data types).
    // The next line constrains the types so that accessing kTextureFormatInfo with
    // a subtype of GPUTextureFormat actually returns nicely a constrained info type
    // (e.g. with `bytesPerBlock: number` instead of `bytesPerBlock: number | undefined`).
    typeof kAllTextureFormatInfo[k];
} = kAllTextureFormatInfo;
/** List of all GPUTextureFormat values. */
/* prettier-ignore */ export const kTextureFormats: readonly GPUTextureFormat[] = keysOf(kAllTextureFormatInfo);

/** Valid GPUTextureFormats for `copyExternalImageToTexture`, by spec. */
export const kValidTextureFormatsForCopyE2T = [
  'r8unorm',
  'r16float',
  'r32float',
  'rg8unorm',
  'rg16float',
  'rg32float',
  'rgba8unorm',
  'rgba8unorm-srgb',
  'bgra8unorm',
  'bgra8unorm-srgb',
  'rgb10a2unorm',
  'rgba16float',
  'rgba32float',
] as const;

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

const kDepthStencilFormatCapabilityInBufferTextureCopy = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    CopyB2T: [],
    CopyT2B: [],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': -1 },
  },
  'depth24plus-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    CopyB2T: ['all', 'depth-only'],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 2, 'stencil-only': -1 },
  },
  depth32float: {
    CopyB2T: [],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': -1 },
  },
  'depth32float-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },
  stencil8: {
    CopyB2T: ['all', 'stencil-only'],
    CopyT2B: ['all', 'stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },
} as const;

/** `kDepthStencilFormatResolvedAspect[format][aspect]` returns the aspect-specific format for a
 *  depth-stencil format, or `undefined` if the format doesn't have the aspect.
 */
export const kDepthStencilFormatResolvedAspect: {
  readonly [k in DepthStencilFormat]: {
    readonly [a in GPUTextureAspect]: DepthStencilFormat | undefined;
  };
} = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    all: 'depth24plus',
    'depth-only': 'depth24plus',
    'stencil-only': undefined,
  },
  'depth24plus-stencil8': {
    all: 'depth24plus-stencil8',
    'depth-only': 'depth24plus',
    'stencil-only': 'stencil8',
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    all: 'depth16unorm',
    'depth-only': 'depth16unorm',
    'stencil-only': undefined,
  },
  depth32float: {
    all: 'depth32float',
    'depth-only': 'depth32float',
    'stencil-only': undefined,
  },
  'depth32float-stencil8': {
    all: 'depth32float-stencil8',
    'depth-only': 'depth32float',
    'stencil-only': 'stencil8',
  },
  stencil8: {
    all: 'stencil8',
    'depth-only': undefined,
    'stencil-only': 'stencil8',
  },
} as const;

/**
 * @returns the GPUTextureFormat corresponding to the @param aspect of @param format.
 * This allows choosing the correct format for depth-stencil aspects when creating pipelines that
 * will have to match the resolved format of views, or to get per-aspect information like the
 * `blockByteSize`.
 *
 * Many helpers use an `undefined` `aspect` to means `'all'` so this is also the default for this
 * function.
 */
export function resolvePerAspectFormat(
  format: GPUTextureFormat,
  aspect?: GPUTextureAspect
): GPUTextureFormat {
  if (aspect === 'all' || aspect === undefined) {
    return format;
  }
  assert(kTextureFormatInfo[format].depth || kTextureFormatInfo[format].stencil);
  const resolved = kDepthStencilFormatResolvedAspect[format as DepthStencilFormat][aspect ?? 'all'];
  assert(resolved !== undefined);
  return resolved;
}

/**
 * Gets all copyable aspects for copies between texture and buffer for specified depth/stencil format and copy type, by spec.
 */
export function depthStencilFormatCopyableAspects(
  type: ImageCopyType,
  format: DepthStencilFormat
): readonly GPUTextureAspect[] {
  const appliedType = type === 'WriteTexture' ? 'CopyB2T' : type;
  return kDepthStencilFormatCapabilityInBufferTextureCopy[format][appliedType];
}

/**
 * Computes whether a copy between a depth/stencil texture aspect and a buffer is supported, by spec.
 */
export function depthStencilBufferTextureCopySupported(
  type: ImageCopyType,
  format: DepthStencilFormat,
  aspect: GPUTextureAspect
): boolean {
  const supportedAspects: readonly GPUTextureAspect[] = depthStencilFormatCopyableAspects(
    type,
    format
  );
  return supportedAspects.includes(aspect);
}

/**
 * Returns the byte size of the depth or stencil aspect of the specified depth/stencil format,
 * or -1 if none.
 */
export function depthStencilFormatAspectSize(
  format: DepthStencilFormat,
  aspect: 'depth-only' | 'stencil-only'
) {
  const texelAspectSize =
    kDepthStencilFormatCapabilityInBufferTextureCopy[format].texelAspectSize[aspect];
  assert(texelAspectSize > 0);
  return texelAspectSize;
}

/**
 * Returns true iff a texture can be created with the provided GPUTextureDimension
 * (defaulting to 2d) and GPUTextureFormat, by spec.
 */
export function textureDimensionAndFormatCompatible(
  dimension: undefined | GPUTextureDimension,
  format: GPUTextureFormat
): boolean {
  const info = kAllTextureFormatInfo[format];
  return !(
    (dimension === '1d' || dimension === '3d') &&
    (info.blockWidth > 1 || info.depth || info.stencil)
  );
}

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
    { texture: { multisampled: true } },
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

/**
 * List of all possible texture sampleCount values.
 *
 * MAINTENANCE_TODO: Switch existing tests to use kTextureSampleCounts
 */
export const kTextureSampleCounts = [1, 4] as const;

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
  'depth-clip-control': {},
  'depth32float-stencil8': {},
  'texture-compression-bc': {},
  'texture-compression-etc2': {},
  'texture-compression-astc': {},
  'timestamp-query': {},
  'indirect-first-instance': {},
  'shader-f16': {},
  'rg11b10ufloat-renderable': {},
};
/** List of all GPUFeatureName values. */
export const kFeatureNames = keysOf(kFeatureNameInfo);

/**
 * Check if two formats are view format compatible.
 *
 * This function may need to be generalized to use `baseFormat` from `kTextureFormatInfo`.
 */
export function viewCompatible(a: GPUTextureFormat, b: GPUTextureFormat): boolean {
  return a === b || a + '-srgb' === b || b + '-srgb' === a;
}

export function getFeaturesForFormats<T>(
  formats: readonly (T & (GPUTextureFormat | undefined))[]
): readonly (GPUFeatureName | undefined)[] {
  return Array.from(new Set(formats.map(f => (f ? kTextureFormatInfo[f].feature : undefined))));
}

export function filterFormatsByFeature<T>(
  feature: GPUFeatureName | undefined,
  formats: readonly (T & (GPUTextureFormat | undefined))[]
): readonly (T & (GPUTextureFormat | undefined))[] {
  return formats.filter(f => f === undefined || kTextureFormatInfo[f].feature === feature);
}

export const kFeaturesForFormats = getFeaturesForFormats(kTextureFormats);
