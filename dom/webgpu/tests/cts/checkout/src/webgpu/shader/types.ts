import { keysOf } from '../../common/util/data_tables.js';
import { assert } from '../../common/util/util.js';
import { align } from '../util/math.js';

const kArrayLength = 3;

export type Requirement = 'never' | 'may' | 'must'; // never is the same as "must not"
export type ContainerType = 'scalar' | 'vector' | 'matrix' | 'atomic' | 'array';
export type ScalarType = 'i32' | 'u32' | 'f32' | 'bool';

export const HostSharableTypes = ['i32', 'u32', 'f32'] as const;

/** Info for each plain scalar type. */
export const kScalarTypeInfo = /* prettier-ignore */ {
  'i32':    { layout: { alignment:  4, size:  4 }, supportsAtomics:  true, arrayLength: 1, innerLength: 0 },
  'u32':    { layout: { alignment:  4, size:  4 }, supportsAtomics:  true, arrayLength: 1, innerLength: 0 },
  'f32':    { layout: { alignment:  4, size:  4 }, supportsAtomics: false, arrayLength: 1, innerLength: 0 },
  'bool':   { layout:                   undefined, supportsAtomics: false, arrayLength: 1, innerLength: 0 },
} as const;
/** List of all plain scalar types. */
export const kScalarTypes = keysOf(kScalarTypeInfo);

/** Info for each vecN<> container type. */
export const kVectorContainerTypeInfo = /* prettier-ignore */ {
  'vec2':   { layout: { alignment:  8, size:  8 }, arrayLength: 2 , innerLength: 0 },
  'vec3':   { layout: { alignment: 16, size: 12 }, arrayLength: 3 , innerLength: 0 },
  'vec4':   { layout: { alignment: 16, size: 16 }, arrayLength: 4 , innerLength: 0 },
} as const;
/** List of all vecN<> container types. */
export const kVectorContainerTypes = keysOf(kVectorContainerTypeInfo);

/** Info for each matNxN<> container type. */
export const kMatrixContainerTypeInfo = /* prettier-ignore */ {
  'mat2x2': { layout: { alignment:  8, size: 16 }, arrayLength: 2, innerLength: 2 },
  'mat3x2': { layout: { alignment:  8, size: 24 }, arrayLength: 3, innerLength: 2 },
  'mat4x2': { layout: { alignment:  8, size: 32 }, arrayLength: 4, innerLength: 2 },
  'mat2x3': { layout: { alignment: 16, size: 32 }, arrayLength: 2, innerLength: 3 },
  'mat3x3': { layout: { alignment: 16, size: 48 }, arrayLength: 3, innerLength: 3 },
  'mat4x3': { layout: { alignment: 16, size: 64 }, arrayLength: 4, innerLength: 3 },
  'mat2x4': { layout: { alignment: 16, size: 32 }, arrayLength: 2, innerLength: 4 },
  'mat3x4': { layout: { alignment: 16, size: 48 }, arrayLength: 3, innerLength: 4 },
  'mat4x4': { layout: { alignment: 16, size: 64 }, arrayLength: 4, innerLength: 4 },
} as const;
/** List of all matNxN<> container types. */
export const kMatrixContainerTypes = keysOf(kMatrixContainerTypeInfo);

export type AddressSpace = 'storage' | 'uniform' | 'private' | 'function' | 'workgroup' | 'handle';
export type AccessMode = 'read' | 'write' | 'read_write';
export type Scope = 'module' | 'function';

export const kAccessModeInfo = {
  read: { read: true, write: false },
  write: { read: false, write: true },
  read_write: { read: true, write: true },
} as const;

export type AddressSpaceInfo = {
  // Variables in this address space must be declared in what scope?
  scope: Scope;

  // True if a variable in this address space requires a binding.
  binding: boolean;

  // Spell the address space in var declarations?
  spell: Requirement;

  // Access modes for ordinary accesses (loads, stores).
  // The first one is the default.
  // This is empty for the 'handle' address space where access is opaque.
  accessModes: readonly AccessMode[];

  // Spell the access mode in var declarations?
  //   7.3 var Declarations
  //   The access mode always has a default value, and except for variables
  //   in the storage address space, must not be specified in the WGSL source.
  //   See §13.3 Address Spaces.
  spellAccessMode: Requirement;
};

export const kAddressSpaceInfo: Record<string, AddressSpaceInfo> = {
  storage: {
    scope: 'module',
    binding: true,
    spell: 'must',
    accessModes: ['read', 'read_write'],
    spellAccessMode: 'may',
  },
  uniform: {
    scope: 'module',
    binding: true,
    spell: 'must',
    accessModes: ['read'],
    spellAccessMode: 'never',
  },
  private: {
    scope: 'module',
    binding: false,
    spell: 'must',
    accessModes: ['read_write'],
    spellAccessMode: 'never',
  },
  workgroup: {
    scope: 'module',
    binding: false,
    spell: 'must',
    accessModes: ['read_write'],
    spellAccessMode: 'never',
  },
  function: {
    scope: 'function',
    binding: false,
    spell: 'may',
    accessModes: ['read_write'],
    spellAccessMode: 'never',
  },
  handle: {
    scope: 'module',
    binding: true,
    spell: 'never',
    accessModes: [],
    spellAccessMode: 'never',
  },
} as const;

/** List of texel formats and their shader representation */
export const TexelFormats = [
  { format: 'rgba8unorm', _shaderType: 'f32' },
  { format: 'rgba8snorm', _shaderType: 'f32' },
  { format: 'rgba8uint', _shaderType: 'u32' },
  { format: 'rgba8sint', _shaderType: 'i32' },
  { format: 'rgba16uint', _shaderType: 'u32' },
  { format: 'rgba16sint', _shaderType: 'i32' },
  { format: 'rgba16float', _shaderType: 'f32' },
  { format: 'r32uint', _shaderType: 'u32' },
  { format: 'r32sint', _shaderType: 'i32' },
  { format: 'r32float', _shaderType: 'f32' },
  { format: 'rg32uint', _shaderType: 'u32' },
  { format: 'rg32sint', _shaderType: 'i32' },
  { format: 'rg32float', _shaderType: 'f32' },
  { format: 'rgba32uint', _shaderType: 'i32' },
  { format: 'rgba32sint', _shaderType: 'i32' },
  { format: 'rgba32float', _shaderType: 'f32' },
] as const;

/**
 * Generate a bunch types (vec, mat, sized/unsized array) for testing.
 */
export function* generateTypes({
  addressSpace,
  baseType,
  containerType,
  isAtomic = false,
}: {
  addressSpace: AddressSpace;
  /** Base scalar type (i32/u32/f32/bool). */
  baseType: ScalarType;
  /** Container type (scalar/vector/matrix/array) */
  containerType: ContainerType;
  /** Whether to wrap the baseType in `atomic<>`. */
  isAtomic?: boolean;
}) {
  const scalarInfo = kScalarTypeInfo[baseType];
  if (isAtomic) {
    assert(scalarInfo.supportsAtomics, 'type does not support atomics');
  }
  const scalarType = isAtomic ? `atomic<${baseType}>` : baseType;

  // Storage and uniform require host-sharable types.
  if (addressSpace === 'storage' || addressSpace === 'uniform') {
    assert(isHostSharable(baseType), 'type ' + baseType.toString() + ' is not host sharable');
  }

  // Scalar types
  if (containerType === 'scalar') {
    yield {
      type: `${scalarType}`,
      _kTypeInfo: {
        elementBaseType: `${scalarType}`,
        ...scalarInfo,
      },
    };
  }

  // Vector types
  if (containerType === 'vector') {
    for (const vectorType of kVectorContainerTypes) {
      yield {
        type: `${vectorType}<${scalarType}>`,
        _kTypeInfo: { elementBaseType: baseType, ...kVectorContainerTypeInfo[vectorType] },
      };
    }
  }

  if (containerType === 'matrix') {
    // Matrices can only be f32.
    if (baseType === 'f32') {
      for (const matrixType of kMatrixContainerTypes) {
        const matrixInfo = kMatrixContainerTypeInfo[matrixType];
        yield {
          type: `${matrixType}<${scalarType}>`,
          _kTypeInfo: {
            elementBaseType: `vec${matrixInfo.innerLength}<${scalarType}>`,
            ...matrixInfo,
          },
        };
      }
    }
  }

  // Array types
  if (containerType === 'array') {
    const arrayTypeInfo = {
      elementBaseType: `${baseType}`,
      arrayLength: kArrayLength,
      layout: scalarInfo.layout
        ? {
            alignment: scalarInfo.layout.alignment,
            size:
              addressSpace === 'uniform'
                ? // Uniform storage class must have array elements aligned to 16.
                  kArrayLength *
                  arrayStride({
                    ...scalarInfo.layout,
                    alignment: 16,
                  })
                : kArrayLength * arrayStride(scalarInfo.layout),
          }
        : undefined,
    };

    // Sized
    if (addressSpace === 'uniform') {
      yield {
        type: `array<vec4<${scalarType}>,${kArrayLength}>`,
        _kTypeInfo: arrayTypeInfo,
      };
    } else {
      yield { type: `array<${scalarType},${kArrayLength}>`, _kTypeInfo: arrayTypeInfo };
    }
    // Unsized
    if (addressSpace === 'storage') {
      yield { type: `array<${scalarType}>`, _kTypeInfo: arrayTypeInfo };
    }
  }

  function arrayStride(elementLayout: { size: number; alignment: number }) {
    return align(elementLayout.size, elementLayout.alignment);
  }

  function isHostSharable(baseType: ScalarType) {
    for (const sharableType of HostSharableTypes) {
      if (sharableType === baseType) return true;
    }
    return false;
  }
}

/** Atomic access requires scalar/array container type and storage/workgroup memory. */
export function supportsAtomics(p: {
  addressSpace: string;
  storageMode: AccessMode | undefined;
  access: string;
  containerType: ContainerType;
}) {
  return (
    ((p.addressSpace === 'storage' && p.storageMode === 'read_write') ||
      p.addressSpace === 'workgroup') &&
    (p.containerType === 'scalar' || p.containerType === 'array')
  );
}

/** Generates an iterator of supported base types (i32/u32/f32/bool) */
export function* supportedScalarTypes(p: { isAtomic: boolean; addressSpace: string }) {
  for (const scalarType of kScalarTypes) {
    const info = kScalarTypeInfo[scalarType];

    // Test atomics only on supported scalar types.
    if (p.isAtomic && !info.supportsAtomics) continue;

    // Storage and uniform require host-sharable types.
    const isHostShared = p.addressSpace === 'storage' || p.addressSpace === 'uniform';
    if (isHostShared && info.layout === undefined) continue;

    yield scalarType;
  }
}
