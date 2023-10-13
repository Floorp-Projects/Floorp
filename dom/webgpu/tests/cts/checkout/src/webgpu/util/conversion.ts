import { Colors } from '../../common/util/colors.js';
import { assert, objectEquals, TypedArrayBufferView, unreachable } from '../../common/util/util.js';
import { Float16Array } from '../../external/petamoriken/float16/float16.js';

import { kBit } from './constants.js';
import {
  cartesianProduct,
  clamp,
  correctlyRoundedF16,
  isFiniteF16,
  isSubnormalNumberF16,
  isSubnormalNumberF32,
  isSubnormalNumberF64,
} from './math.js';

/**
 * Encodes a JS `number` into a "normalized" (unorm/snorm) integer representation with `bits` bits.
 * Input must be between -1 and 1 if signed, or 0 and 1 if unsigned.
 *
 * MAINTENANCE_TODO: See if performance of texel_data improves if this function is pre-specialized
 * for a particular `bits`/`signed`.
 */
export function floatAsNormalizedInteger(float: number, bits: number, signed: boolean): number {
  if (signed) {
    assert(float >= -1 && float <= 1, () => `${float} out of bounds of snorm`);
    const max = Math.pow(2, bits - 1) - 1;
    return Math.round(float * max);
  } else {
    assert(float >= 0 && float <= 1, () => `${float} out of bounds of unorm`);
    const max = Math.pow(2, bits) - 1;
    return Math.round(float * max);
  }
}

/**
 * Decodes a JS `number` from a "normalized" (unorm/snorm) integer representation with `bits` bits.
 * Input must be an integer in the range of the specified unorm/snorm type.
 */
export function normalizedIntegerAsFloat(integer: number, bits: number, signed: boolean): number {
  assert(Number.isInteger(integer));
  if (signed) {
    const max = Math.pow(2, bits - 1) - 1;
    assert(integer >= -max - 1 && integer <= max);
    if (integer === -max - 1) {
      integer = -max;
    }
    return integer / max;
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(integer >= 0 && integer <= max);
    return integer / max;
  }
}

/**
 * Compares 2 numbers. Returns true if their absolute value is
 * less than or equal to maxDiff or if they are both NaN or the
 * same sign infinity.
 */
export function numbersApproximatelyEqual(a: number, b: number, maxDiff: number = 0) {
  return (
    (Number.isNaN(a) && Number.isNaN(b)) ||
    (a === Number.POSITIVE_INFINITY && b === Number.POSITIVE_INFINITY) ||
    (a === Number.NEGATIVE_INFINITY && b === Number.NEGATIVE_INFINITY) ||
    Math.abs(a - b) <= maxDiff
  );
}

/**
 * Encodes a JS `number` into an IEEE754 floating point number with the specified number of
 * sign, exponent, mantissa bits, and exponent bias.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 *
 * MAINTENANCE_TODO: Replace usages of this with numberToFloatBits.
 */
export function float32ToFloatBits(
  n: number,
  signBits: 0 | 1,
  exponentBits: number,
  mantissaBits: number,
  bias: number
): number {
  assert(exponentBits <= 8);
  assert(mantissaBits <= 23);

  if (Number.isNaN(n)) {
    // NaN = all exponent bits true, 1 or more mantissia bits true
    return (((1 << exponentBits) - 1) << mantissaBits) | ((1 << mantissaBits) - 1);
  }

  const buf = new DataView(new ArrayBuffer(Float32Array.BYTES_PER_ELEMENT));
  buf.setFloat32(0, n, true);
  const bits = buf.getUint32(0, true);
  // bits (32): seeeeeeeefffffffffffffffffffffff

  // 0 or 1
  const sign = (bits >> 31) & signBits;

  if (n === 0) {
    if (sign === 1) {
      // Handle negative zero.
      return 1 << (exponentBits + mantissaBits);
    }
    return 0;
  }

  if (signBits === 0) {
    assert(n >= 0);
  }

  if (!Number.isFinite(n)) {
    // Infinity = all exponent bits true, no mantissa bits true
    // plus the sign bit.
    return (
      (((1 << exponentBits) - 1) << mantissaBits) | (n < 0 ? 2 ** (exponentBits + mantissaBits) : 0)
    );
  }

  const mantissaBitsToDiscard = 23 - mantissaBits;

  // >> to remove mantissa, & to remove sign, - 127 to remove bias.
  const exp = ((bits >> 23) & 0xff) - 127;

  // Convert to the new biased exponent.
  const newBiasedExp = bias + exp;
  assert(newBiasedExp < 1 << exponentBits, () => `input number ${n} overflows target type`);

  if (newBiasedExp <= 0) {
    // Result is subnormal or zero. Round to (signed) zero.
    return sign << (exponentBits + mantissaBits);
  } else {
    // Mask only the mantissa, and discard the lower bits.
    const newMantissa = (bits & 0x7fffff) >> mantissaBitsToDiscard;
    return (sign << (exponentBits + mantissaBits)) | (newBiasedExp << mantissaBits) | newMantissa;
  }
}

/**
 * Encodes a JS `number` into an IEEE754 16 bit floating point number.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 */
export function float32ToFloat16Bits(n: number) {
  return float32ToFloatBits(n, 1, 5, 10, 15);
}

/**
 * Decodes an IEEE754 16 bit floating point number into a JS `number` and returns.
 */
export function float16BitsToFloat32(float16Bits: number): number {
  return floatBitsToNumber(float16Bits, kFloat16Format);
}

type FloatFormat = { signed: 0 | 1; exponentBits: number; mantissaBits: number; bias: number };

/** FloatFormat defining IEEE754 32-bit float. */
export const kFloat32Format = { signed: 1, exponentBits: 8, mantissaBits: 23, bias: 127 } as const;
/** FloatFormat defining IEEE754 16-bit float. */
export const kFloat16Format = { signed: 1, exponentBits: 5, mantissaBits: 10, bias: 15 } as const;
/** FloatFormat for 9 bit mantissa, 5 bit exponent unsigned float */
export const kUFloat9e5Format = { signed: 0, exponentBits: 5, mantissaBits: 9, bias: 15 } as const;

/**
 * Once-allocated ArrayBuffer/views to avoid overhead of allocation when converting between numeric formats
 *
 * workingData* is shared between multiple functions in this file, so to avoid re-entrancy problems, make sure in
 * functions that use it that they don't call themselves or other functions that use workingData*.
 */
const workingData = new ArrayBuffer(4);
const workingDataU32 = new Uint32Array(workingData);
const workingDataU16 = new Uint16Array(workingData);
const workingDataU8 = new Uint8Array(workingData);
const workingDataF32 = new Float32Array(workingData);
const workingDataF16 = new Float16Array(workingData);
const workingDataI16 = new Int16Array(workingData);
const workingDataI8 = new Int8Array(workingData);

/** Bitcast u32 (represented as integer Number) to f32 (represented as floating-point Number). */
export function float32BitsToNumber(bits: number): number {
  workingDataU32[0] = bits;
  return workingDataF32[0];
}
/** Bitcast f32 (represented as floating-point Number) to u32 (represented as integer Number). */
export function numberToFloat32Bits(number: number): number {
  workingDataF32[0] = number;
  return workingDataU32[0];
}

/**
 * Decodes an IEEE754 float with the supplied format specification into a JS number.
 *
 * The format MUST be no larger than a 32-bit float.
 */
export function floatBitsToNumber(bits: number, fmt: FloatFormat): number {
  // Pad the provided bits out to f32, then convert to a `number` with the wrong bias.
  // E.g. for f16 to f32:
  // - f16: S    EEEEE MMMMMMMMMM
  //        ^ 000^^^^^ ^^^^^^^^^^0000000000000
  // - f32: S eeeEEEEE MMMMMMMMMMmmmmmmmmmmmmm

  const kNonSignBits = fmt.exponentBits + fmt.mantissaBits;
  const kNonSignBitsMask = (1 << kNonSignBits) - 1;
  const exponentAndMantissaBits = bits & kNonSignBitsMask;
  const exponentMask = ((1 << fmt.exponentBits) - 1) << fmt.mantissaBits;
  const infinityOrNaN = (bits & exponentMask) === exponentMask;
  if (infinityOrNaN) {
    const mantissaMask = (1 << fmt.mantissaBits) - 1;
    const signBit = 2 ** kNonSignBits;
    const isNegative = (bits & signBit) !== 0;
    return bits & mantissaMask
      ? Number.NaN
      : isNegative
      ? Number.NEGATIVE_INFINITY
      : Number.POSITIVE_INFINITY;
  }
  let f32BitsWithWrongBias =
    exponentAndMantissaBits << (kFloat32Format.mantissaBits - fmt.mantissaBits);
  f32BitsWithWrongBias |= (bits << (31 - kNonSignBits)) & 0x8000_0000;
  const numberWithWrongBias = float32BitsToNumber(f32BitsWithWrongBias);
  return numberWithWrongBias * 2 ** (kFloat32Format.bias - fmt.bias);
}

/**
 * Convert ufloat9e5 bits from rgb9e5ufloat to a JS number
 *
 * The difference between `floatBitsToNumber` and `ufloatBitsToNumber`
 * is that the latter doesn't use an implicit leading bit:
 *
 * floatBitsToNumber      = 2^(exponent - bias) * (1 + mantissa / 2 ^ numMantissaBits)
 * ufloatM9E5BitsToNumber = 2^(exponent - bias) * (mantissa / 2 ^ numMantissaBits)
 *                        = 2^(exponent - bias - numMantissaBits) * mantissa
 */
export function ufloatM9E5BitsToNumber(bits: number, fmt: FloatFormat): number {
  const exponent = bits >> fmt.mantissaBits;
  const mantissaMask = (1 << fmt.mantissaBits) - 1;
  const mantissa = bits & mantissaMask;
  return mantissa * 2 ** (exponent - fmt.bias - fmt.mantissaBits);
}

/**
 * Encodes a JS `number` into an IEEE754 floating point number with the specified format.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 */
export function numberToFloatBits(number: number, fmt: FloatFormat): number {
  return float32ToFloatBits(number, fmt.signed, fmt.exponentBits, fmt.mantissaBits, fmt.bias);
}

/**
 * Given a floating point number (as an integer representing its bits), computes how many ULPs it is
 * from zero.
 *
 * Subnormal numbers are skipped, so that 0 is one ULP from the minimum normal number.
 * Subnormal values are flushed to 0.
 * Positive and negative 0 are both considered to be 0 ULPs from 0.
 */
export function floatBitsToNormalULPFromZero(bits: number, fmt: FloatFormat): number {
  const mask_sign = fmt.signed << (fmt.exponentBits + fmt.mantissaBits);
  const mask_expt = ((1 << fmt.exponentBits) - 1) << fmt.mantissaBits;
  const mask_mant = (1 << fmt.mantissaBits) - 1;
  const mask_rest = mask_expt | mask_mant;

  assert(fmt.exponentBits + fmt.mantissaBits <= 31);

  const sign = bits & mask_sign ? -1 : 1;
  const rest = bits & mask_rest;
  const subnormal_or_zero = (bits & mask_expt) === 0;
  const infinity_or_nan = (bits & mask_expt) === mask_expt;
  assert(!infinity_or_nan, 'no ulp representation for infinity/nan');

  // The first normal number is mask_mant+1, so subtract mask_mant to make min_normal - zero = 1ULP.
  const abs_ulp_from_zero = subnormal_or_zero ? 0 : rest - mask_mant;
  return sign * abs_ulp_from_zero;
}

/**
 * Encodes three JS `number` values into RGB9E5, returned as an integer-valued JS `number`.
 *
 * RGB9E5 represents three partial-precision floating-point numbers encoded into a single 32-bit
 * value all sharing the same 5-bit exponent.
 * There is no sign bit, and there is a shared 5-bit biased (15) exponent and a 9-bit
 * mantissa for each channel. The mantissa does NOT have an implicit leading "1.",
 * and instead has an implicit leading "0.".
 *
 * @see https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
 */
export function packRGB9E5UFloat(r: number, g: number, b: number): number {
  const N = 9; // number of mantissa bits
  const Emax = 31; // max exponent
  const B = 15; // exponent bias
  const sharedexp_max = (((1 << N) - 1) / (1 << N)) * 2 ** (Emax - B);
  const red_c = clamp(r, { min: 0, max: sharedexp_max });
  const green_c = clamp(g, { min: 0, max: sharedexp_max });
  const blue_c = clamp(b, { min: 0, max: sharedexp_max });
  const max_c = Math.max(red_c, green_c, blue_c);
  const exp_shared_p = Math.max(-B - 1, Math.floor(Math.log2(max_c))) + 1 + B;
  const max_s = Math.floor(max_c / 2 ** (exp_shared_p - B - N) + 0.5);
  const exp_shared = max_s === 1 << N ? exp_shared_p + 1 : exp_shared_p;
  const scalar = 1 / 2 ** (exp_shared - B - N);
  const red_s = Math.floor(red_c * scalar + 0.5);
  const green_s = Math.floor(green_c * scalar + 0.5);
  const blue_s = Math.floor(blue_c * scalar + 0.5);
  assert(red_s >= 0 && red_s <= 0b111111111);
  assert(green_s >= 0 && green_s <= 0b111111111);
  assert(blue_s >= 0 && blue_s <= 0b111111111);
  assert(exp_shared >= 0 && exp_shared <= 0b11111);
  return ((exp_shared << 27) | (blue_s << 18) | (green_s << 9) | red_s) >>> 0;
}

/**
 * Decodes a RGB9E5 encoded color.
 * @see packRGB9E5UFloat
 */
export function unpackRGB9E5UFloat(encoded: number): { R: number; G: number; B: number } {
  const N = 9; // number of mantissa bits
  const B = 15; // exponent bias
  const red_s = (encoded >>> 0) & 0b111111111;
  const green_s = (encoded >>> 9) & 0b111111111;
  const blue_s = (encoded >>> 18) & 0b111111111;
  const exp_shared = (encoded >>> 27) & 0b11111;
  const exp = Math.pow(2, exp_shared - B - N);
  return {
    R: exp * red_s,
    G: exp * green_s,
    B: exp * blue_s,
  };
}

/**
 * Quantizes two f32s to f16 and then packs them in a u32
 *
 * This should implement the same behaviour as the builtin `pack2x16float` from
 * WGSL.
 *
 * Caller is responsible to ensuring inputs are f32s
 *
 * @param x first f32 to be packed
 * @param y second f32 to be packed
 * @returns an array of possible results for pack2x16float. Elements are either
 *          a number or undefined.
 *          undefined indicates that any value is valid, since the input went
 *          out of bounds.
 */
export function pack2x16float(x: number, y: number): (number | undefined)[] {
  // Generates all possible valid u16 bit fields for a given f32 to f16 conversion.
  // Assumes FTZ for both the f32 and f16 value is allowed.
  const generateU16s = (n: number): number[] => {
    let contains_subnormals = isSubnormalNumberF32(n);
    const n_f16s = correctlyRoundedF16(n);
    contains_subnormals ||= n_f16s.some(isSubnormalNumberF16);

    const n_u16s = n_f16s.map(f16 => {
      workingDataF16[0] = f16;
      return workingDataU16[0];
    });

    const contains_poszero = n_u16s.some(u => u === kBit.f16.positive.zero);
    const contains_negzero = n_u16s.some(u => u === kBit.f16.negative.zero);
    if (!contains_negzero && (contains_poszero || contains_subnormals)) {
      n_u16s.push(kBit.f16.negative.zero);
    }

    if (!contains_poszero && (contains_negzero || contains_subnormals)) {
      n_u16s.push(kBit.f16.positive.zero);
    }

    return n_u16s;
  };

  if (!isFiniteF16(x) || !isFiniteF16(y)) {
    // This indicates any value is valid, so it isn't worth bothering
    // calculating the more restrictive possibilities.
    return [undefined];
  }

  const results = new Array<number>();
  for (const p of cartesianProduct(generateU16s(x), generateU16s(y))) {
    assert(p.length === 2, 'cartesianProduct of 2 arrays returned an entry with not 2 elements');
    workingDataU16[0] = p[0];
    workingDataU16[1] = p[1];
    results.push(workingDataU32[0]);
  }

  return results;
}

/**
 * Converts two normalized f32s to i16s and then packs them in a u32
 *
 * This should implement the same behaviour as the builtin `pack2x16snorm` from
 * WGSL.
 *
 * Caller is responsible to ensuring inputs are normalized f32s
 *
 * @param x first f32 to be packed
 * @param y second f32 to be packed
 * @returns a number that is expected result of pack2x16snorm.
 */
export function pack2x16snorm(x: number, y: number): number {
  // Converts f32 to i16 via the pack2x16snorm formula.
  // FTZ is not explicitly handled, because all subnormals will produce a value
  // between 0 and 1, but significantly away from the edges, so floor goes to 0.
  const generateI16 = (n: number): number => {
    return Math.floor(0.5 + 32767 * Math.min(1, Math.max(-1, n)));
  };

  workingDataI16[0] = generateI16(x);
  workingDataI16[1] = generateI16(y);

  return workingDataU32[0];
}

/**
 * Converts two normalized f32s to u16s and then packs them in a u32
 *
 * This should implement the same behaviour as the builtin `pack2x16unorm` from
 * WGSL.
 *
 * Caller is responsible to ensuring inputs are normalized f32s
 *
 * @param x first f32 to be packed
 * @param y second f32 to be packed
 * @returns an number that is expected result of pack2x16unorm.
 */
export function pack2x16unorm(x: number, y: number): number {
  // Converts f32 to u16 via the pack2x16unorm formula.
  // FTZ is not explicitly handled, because all subnormals will produce a value
  // between 0.5 and much less than 1, so floor goes to 0.
  const generateU16 = (n: number): number => {
    return Math.floor(0.5 + 65535 * Math.min(1, Math.max(0, n)));
  };

  workingDataU16[0] = generateU16(x);
  workingDataU16[1] = generateU16(y);

  return workingDataU32[0];
}

/**
 * Converts four normalized f32s to i8s and then packs them in a u32
 *
 * This should implement the same behaviour as the builtin `pack4x8snorm` from
 * WGSL.
 *
 * Caller is responsible to ensuring inputs are normalized f32s
 *
 * @param vals four f32s to be packed
 * @returns a number that is expected result of pack4x8usorm.
 */
export function pack4x8snorm(...vals: [number, number, number, number]): number {
  // Converts f32 to u8 via the pack4x8snorm formula.
  // FTZ is not explicitly handled, because all subnormals will produce a value
  // between 0 and 1, so floor goes to 0.
  const generateI8 = (n: number): number => {
    return Math.floor(0.5 + 127 * Math.min(1, Math.max(-1, n)));
  };

  for (const idx in vals) {
    workingDataI8[idx] = generateI8(vals[idx]);
  }

  return workingDataU32[0];
}

/**
 * Converts four normalized f32s to u8s and then packs them in a u32
 *
 * This should implement the same behaviour as the builtin `pack4x8unorm` from
 * WGSL.
 *
 * Caller is responsible to ensuring inputs are normalized f32s
 *
 * @param vals four f32s to be packed
 * @returns a number that is expected result of pack4x8unorm.
 */
export function pack4x8unorm(...vals: [number, number, number, number]): number {
  // Converts f32 to u8 via the pack4x8unorm formula.
  // FTZ is not explicitly handled, because all subnormals will produce a value
  // between 0.5 and much less than 1, so floor goes to 0.
  const generateU8 = (n: number): number => {
    return Math.floor(0.5 + 255 * Math.min(1, Math.max(0, n)));
  };

  for (const idx in vals) {
    workingDataU8[idx] = generateU8(vals[idx]);
  }

  return workingDataU32[0];
}

/**
 * Asserts that a number is within the representable (inclusive) of the integer type with the
 * specified number of bits and signedness.
 *
 * MAINTENANCE_TODO: Assert isInteger? Then this function "asserts that a number is representable"
 * by the type.
 */
export function assertInIntegerRange(n: number, bits: number, signed: boolean): void {
  if (signed) {
    const min = -Math.pow(2, bits - 1);
    const max = Math.pow(2, bits - 1) - 1;
    assert(n >= min && n <= max);
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(n >= 0 && n <= max);
  }
}

/**
 * Converts a linear value into a "gamma"-encoded value using the sRGB-clamped transfer function.
 */
export function gammaCompress(n: number): number {
  n = n <= 0.0031308 ? (323 * n) / 25 : (211 * Math.pow(n, 5 / 12) - 11) / 200;
  return clamp(n, { min: 0, max: 1 });
}

/**
 * Converts a "gamma"-encoded value into a linear value using the sRGB-clamped transfer function.
 */
export function gammaDecompress(n: number): number {
  n = n <= 0.04045 ? (n * 25) / 323 : Math.pow((200 * n + 11) / 211, 12 / 5);
  return clamp(n, { min: 0, max: 1 });
}

/** Converts a 32-bit float value to a 32-bit unsigned integer value */
export function float32ToUint32(f32: number): number {
  const f32Arr = new Float32Array(1);
  f32Arr[0] = f32;
  const u32Arr = new Uint32Array(f32Arr.buffer);
  return u32Arr[0];
}

/** Converts a 32-bit unsigned integer value to a 32-bit float value */
export function uint32ToFloat32(u32: number): number {
  const u32Arr = new Uint32Array(1);
  u32Arr[0] = u32;
  const f32Arr = new Float32Array(u32Arr.buffer);
  return f32Arr[0];
}

/** Converts a 32-bit float value to a 32-bit signed integer value */
export function float32ToInt32(f32: number): number {
  const f32Arr = new Float32Array(1);
  f32Arr[0] = f32;
  const i32Arr = new Int32Array(f32Arr.buffer);
  return i32Arr[0];
}

/** Converts a 32-bit unsigned integer value to a 32-bit signed integer value */
export function uint32ToInt32(u32: number): number {
  const u32Arr = new Uint32Array(1);
  u32Arr[0] = u32;
  const i32Arr = new Int32Array(u32Arr.buffer);
  return i32Arr[0];
}

/** Converts a 16-bit float value to a 16-bit unsigned integer value */
export function float16ToUint16(f16: number): number {
  const f16Arr = new Float16Array(1);
  f16Arr[0] = f16;
  const u16Arr = new Uint16Array(f16Arr.buffer);
  return u16Arr[0];
}

/** Converts a 16-bit unsigned integer value to a 16-bit float value */
export function uint16ToFloat16(u16: number): number {
  const u16Arr = new Uint16Array(1);
  u16Arr[0] = u16;
  const f16Arr = new Float16Array(u16Arr.buffer);
  return f16Arr[0];
}

/** Converts a 16-bit float value to a 16-bit signed integer value */
export function float16ToInt16(f16: number): number {
  const f16Arr = new Float16Array(1);
  f16Arr[0] = f16;
  const i16Arr = new Int16Array(f16Arr.buffer);
  return i16Arr[0];
}

/** A type of number representable by Scalar. */
export type ScalarKind =
  | 'abstract-float'
  | 'f64'
  | 'f32'
  | 'f16'
  | 'u32'
  | 'u16'
  | 'u8'
  | 'i32'
  | 'i16'
  | 'i8'
  | 'bool';

/** ScalarType describes the type of WGSL Scalar. */
export class ScalarType {
  readonly kind: ScalarKind; // The named type
  readonly _size: number; // In bytes
  readonly read: (buf: Uint8Array, offset: number) => Scalar; // reads a scalar from a buffer

  constructor(kind: ScalarKind, size: number, read: (buf: Uint8Array, offset: number) => Scalar) {
    this.kind = kind;
    this._size = size;
    this.read = read;
  }

  public toString(): string {
    return this.kind;
  }

  public get size(): number {
    return this._size;
  }

  /** Constructs a Scalar of this type with @p value */
  public create(value: number): Scalar {
    switch (this.kind) {
      case 'abstract-float':
        return abstractFloat(value);
      case 'f64':
        return f64(value);
      case 'f32':
        return f32(value);
      case 'f16':
        return f16(value);
      case 'u32':
        return u32(value);
      case 'u16':
        return u16(value);
      case 'u8':
        return u8(value);
      case 'i32':
        return i32(value);
      case 'i16':
        return i16(value);
      case 'i8':
        return i8(value);
      case 'bool':
        return bool(value !== 0);
    }
  }
}

/** VectorType describes the type of WGSL Vector. */
export class VectorType {
  readonly width: number; // Number of elements in the vector
  readonly elementType: ScalarType; // Element type

  constructor(width: number, elementType: ScalarType) {
    this.width = width;
    this.elementType = elementType;
  }

  /**
   * @returns a vector constructed from the values read from the buffer at the
   * given byte offset
   */
  public read(buf: Uint8Array, offset: number): Vector {
    const elements: Array<Scalar> = [];
    for (let i = 0; i < this.width; i++) {
      elements[i] = this.elementType.read(buf, offset);
      offset += this.elementType.size;
    }
    return new Vector(elements);
  }

  public toString(): string {
    return `vec${this.width}<${this.elementType}>`;
  }

  public get size(): number {
    return this.elementType.size * this.width;
  }

  /** Constructs a Vector of this type with the given values */
  public create(value: number | number[]): Vector {
    if (value instanceof Array) {
      assert(value.length === this.width);
    } else {
      value = Array(this.width).fill(value);
    }
    return new Vector(value.map(v => this.elementType.create(v)));
  }
}

// Maps a string representation of a vector type to vector type.
const vectorTypes = new Map<string, VectorType>();

export function TypeVec(width: number, elementType: ScalarType): VectorType {
  const key = `${elementType.toString()} ${width}}`;
  let ty = vectorTypes.get(key);
  if (ty !== undefined) {
    return ty;
  }
  ty = new VectorType(width, elementType);
  vectorTypes.set(key, ty);
  return ty;
}

/** MatrixType describes the type of WGSL Matrix. */
export class MatrixType {
  readonly cols: number; // Number of columns in the Matrix
  readonly rows: number; // Number of elements per column in the Matrix
  readonly elementType: ScalarType; // Element type

  constructor(cols: number, rows: number, elementType: ScalarType) {
    this.cols = cols;
    this.rows = rows;
    assert(
      elementType.kind === 'f32' ||
        elementType.kind === 'f16' ||
        elementType.kind === 'abstract-float',
      "MatrixType can only have elementType of 'f32' or 'f16' or 'abstract-float'"
    );
    this.elementType = elementType;
  }

  /**
   * @returns a Matrix constructed from the values read from the buffer at the
   * given byte offset
   */
  public read(buf: Uint8Array, offset: number): Matrix {
    const elements: Scalar[][] = [...Array(this.cols)].map(_ => [...Array(this.rows)]);
    for (let c = 0; c < this.cols; c++) {
      for (let r = 0; r < this.rows; r++) {
        elements[c][r] = this.elementType.read(buf, offset);
        offset += this.elementType.size;
      }

      // vec3 have one padding element, so need to skip in matrices
      if (this.rows === 3) {
        offset += this.elementType.size;
      }
    }
    return new Matrix(elements);
  }

  public toString(): string {
    return `mat${this.cols}x${this.rows}<${this.elementType}>`;
  }
}

// Maps a string representation of a Matrix type to Matrix type.
const matrixTypes = new Map<string, MatrixType>();

export function TypeMat(cols: number, rows: number, elementType: ScalarType): MatrixType {
  const key = `${elementType.toString()} ${cols} ${rows}`;
  let ty = matrixTypes.get(key);
  if (ty !== undefined) {
    return ty;
  }
  ty = new MatrixType(cols, rows, elementType);
  matrixTypes.set(key, ty);
  return ty;
}

/** Type is a ScalarType, VectorType, or MatrixType. */
export type Type = ScalarType | VectorType | MatrixType;

export const TypeI32 = new ScalarType('i32', 4, (buf: Uint8Array, offset: number) =>
  i32(new Int32Array(buf.buffer, offset)[0])
);
export const TypeU32 = new ScalarType('u32', 4, (buf: Uint8Array, offset: number) =>
  u32(new Uint32Array(buf.buffer, offset)[0])
);
export const TypeAbstractFloat = new ScalarType(
  'abstract-float',
  8,
  (buf: Uint8Array, offset: number) => abstractFloat(new Float64Array(buf.buffer, offset)[0])
);
export const TypeF64 = new ScalarType('f64', 8, (buf: Uint8Array, offset: number) =>
  f64(new Float64Array(buf.buffer, offset)[0])
);
export const TypeF32 = new ScalarType('f32', 4, (buf: Uint8Array, offset: number) =>
  f32(new Float32Array(buf.buffer, offset)[0])
);
export const TypeI16 = new ScalarType('i16', 2, (buf: Uint8Array, offset: number) =>
  i16(new Int16Array(buf.buffer, offset)[0])
);
export const TypeU16 = new ScalarType('u16', 2, (buf: Uint8Array, offset: number) =>
  u16(new Uint16Array(buf.buffer, offset)[0])
);
export const TypeF16 = new ScalarType('f16', 2, (buf: Uint8Array, offset: number) =>
  f16Bits(new Uint16Array(buf.buffer, offset)[0])
);
export const TypeI8 = new ScalarType('i8', 1, (buf: Uint8Array, offset: number) =>
  i8(new Int8Array(buf.buffer, offset)[0])
);
export const TypeU8 = new ScalarType('u8', 1, (buf: Uint8Array, offset: number) =>
  u8(new Uint8Array(buf.buffer, offset)[0])
);
export const TypeBool = new ScalarType('bool', 4, (buf: Uint8Array, offset: number) =>
  bool(new Uint32Array(buf.buffer, offset)[0] !== 0)
);

/** @returns the ScalarType from the ScalarKind */
export function scalarType(kind: ScalarKind): ScalarType {
  switch (kind) {
    case 'abstract-float':
      return TypeAbstractFloat;
    case 'f64':
      return TypeF64;
    case 'f32':
      return TypeF32;
    case 'f16':
      return TypeF16;
    case 'u32':
      return TypeU32;
    case 'u16':
      return TypeU16;
    case 'u8':
      return TypeU8;
    case 'i32':
      return TypeI32;
    case 'i16':
      return TypeI16;
    case 'i8':
      return TypeI8;
    case 'bool':
      return TypeBool;
  }
}

/** @returns the number of scalar (element) types of the given Type */
export function numElementsOf(ty: Type): number {
  if (ty instanceof ScalarType) {
    return 1;
  }
  if (ty instanceof VectorType) {
    return ty.width;
  }
  if (ty instanceof MatrixType) {
    return ty.cols * ty.rows;
  }
  throw new Error(`unhandled type ${ty}`);
}

/** @returns the scalar elements of the given Value */
export function elementsOf(value: Value): Scalar[] {
  if (value instanceof Scalar) {
    return [value];
  }
  if (value instanceof Vector) {
    return value.elements;
  }
  if (value instanceof Matrix) {
    return value.elements.flat();
  }
  throw new Error(`unhandled value ${value}`);
}

/** @returns the scalar (element) type of the given Type */
export function scalarTypeOf(ty: Type): ScalarType {
  if (ty instanceof ScalarType) {
    return ty;
  }
  if (ty instanceof VectorType) {
    return ty.elementType;
  }
  if (ty instanceof MatrixType) {
    return ty.elementType;
  }
  throw new Error(`unhandled type ${ty}`);
}

/** ScalarValue is the JS type that can be held by a Scalar */
type ScalarValue = boolean | number;

/** Class that encapsulates a single scalar value of various types. */
export class Scalar {
  readonly value: ScalarValue; // The scalar value
  readonly type: ScalarType; // The type of the scalar
  readonly bits: Uint8Array; // The scalar value packed in a Uint8Array

  public constructor(type: ScalarType, value: ScalarValue, bits: TypedArrayBufferView) {
    this.value = value;
    this.type = type;
    this.bits = new Uint8Array(bits.buffer);
  }

  /**
   * Copies the scalar value to the Uint8Array buffer at the provided byte offset.
   * @param buffer the destination buffer
   * @param offset the byte offset within buffer
   */
  public copyTo(buffer: Uint8Array, offset: number) {
    assert(this.type.kind !== 'f64', `Copying f64 values to/from buffers is not defined`);
    for (let i = 0; i < this.bits.length; i++) {
      buffer[offset + i] = this.bits[i];
    }
  }

  /**
   * @returns the WGSL representation of this scalar value
   */
  public wgsl(): string {
    const withPoint = (x: number) => {
      const str = `${x}`;
      return str.indexOf('.') > 0 || str.indexOf('e') > 0 ? str : `${str}.0`;
    };
    if (isFinite(this.value as number)) {
      switch (this.type.kind) {
        case 'abstract-float':
          return `${withPoint(this.value as number)}`;
        case 'f64':
          return `${withPoint(this.value as number)}`;
        case 'f32':
          return `${withPoint(this.value as number)}f`;
        case 'f16':
          return `${withPoint(this.value as number)}h`;
        case 'u32':
          return `${this.value}u`;
        case 'i32':
          return `i32(${this.value})`;
        case 'bool':
          return `${this.value}`;
      }
    }
    throw new Error(
      `scalar of value ${this.value} and type ${this.type} has no WGSL representation`
    );
  }

  public toString(): string {
    if (this.type.kind === 'bool') {
      return Colors.bold(this.value.toString());
    }
    switch (this.value) {
      case Infinity:
      case -Infinity:
        return Colors.bold(this.value.toString());
      default: {
        // Uint8Array.map returns a Uint8Array, so cannot use .map directly
        const hex = Array.from(this.bits)
          .reverse()
          .map(x => x.toString(16).padStart(2, '0'))
          .join('');
        const n = this.value as Number;
        if (n !== null && isFloatValue(this)) {
          let str = this.value.toString();
          str = str.indexOf('.') > 0 || str.indexOf('e') > 0 ? str : `${str}.0`;
          switch (this.type.kind) {
            case 'abstract-float':
              return isSubnormalNumberF64(n.valueOf())
                ? `${Colors.bold(str)} (0x${hex} subnormal)`
                : `${Colors.bold(str)} (0x${hex})`;
            case 'f64':
              return isSubnormalNumberF64(n.valueOf())
                ? `${Colors.bold(str)} (0x${hex} subnormal)`
                : `${Colors.bold(str)} (0x${hex})`;
            case 'f32':
              return isSubnormalNumberF32(n.valueOf())
                ? `${Colors.bold(str)} (0x${hex} subnormal)`
                : `${Colors.bold(str)} (0x${hex})`;
            case 'f16':
              return isSubnormalNumberF16(n.valueOf())
                ? `${Colors.bold(str)} (0x${hex} subnormal)`
                : `${Colors.bold(str)} (0x${hex})`;
            default:
              unreachable(
                `Printing of floating point kind ${this.type.kind} is not implemented...`
              );
          }
        }
        return `${Colors.bold(this.value.toString())} (0x${hex})`;
      }
    }
  }
}

export interface ScalarBuilder {
  (value: number): Scalar;
}

/** Create an AbstractFloat from a numeric value, a JS `number`. */
export function abstractFloat(value: number): Scalar {
  const arr = new Float64Array([value]);
  return new Scalar(TypeAbstractFloat, arr[0], arr);
}
/** Create an f64 from a numeric value, a JS `number`. */
export function f64(value: number): Scalar {
  const arr = new Float64Array([value]);
  return new Scalar(TypeF64, arr[0], arr);
}
/** Create an f32 from a numeric value, a JS `number`. */
export function f32(value: number): Scalar {
  const arr = new Float32Array([value]);
  return new Scalar(TypeF32, arr[0], arr);
}
/** Create an f16 from a numeric value, a JS `number`. */
export function f16(value: number): Scalar {
  const arr = new Float16Array([value]);
  return new Scalar(TypeF16, arr[0], arr);
}
/** Create an f32 from a bit representation, a uint32 represented as a JS `number`. */
export function f32Bits(bits: number): Scalar {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeF32, new Float32Array(arr.buffer)[0], arr);
}
/** Create an f16 from a bit representation, a uint16 represented as a JS `number`. */
export function f16Bits(bits: number): Scalar {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeF16, new Float16Array(arr.buffer)[0], arr);
}

/** Create an i32 from a numeric value, a JS `number`. */
export function i32(value: number): Scalar {
  const arr = new Int32Array([value]);
  return new Scalar(TypeI32, arr[0], arr);
}
/** Create an i16 from a numeric value, a JS `number`. */
export function i16(value: number): Scalar {
  const arr = new Int16Array([value]);
  return new Scalar(TypeI16, arr[0], arr);
}
/** Create an i8 from a numeric value, a JS `number`. */
export function i8(value: number): Scalar {
  const arr = new Int8Array([value]);
  return new Scalar(TypeI8, arr[0], arr);
}

/** Create an i32 from a bit representation, a uint32 represented as a JS `number`. */
export function i32Bits(bits: number): Scalar {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeI32, new Int32Array(arr.buffer)[0], arr);
}
/** Create an i16 from a bit representation, a uint16 represented as a JS `number`. */
export function i16Bits(bits: number): Scalar {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeI16, new Int16Array(arr.buffer)[0], arr);
}
/** Create an i8 from a bit representation, a uint8 represented as a JS `number`. */
export function i8Bits(bits: number): Scalar {
  const arr = new Uint8Array([bits]);
  return new Scalar(TypeI8, new Int8Array(arr.buffer)[0], arr);
}

/** Create a u32 from a numeric value, a JS `number`. */
export function u32(value: number): Scalar {
  const arr = new Uint32Array([value]);
  return new Scalar(TypeU32, arr[0], arr);
}
/** Create a u16 from a numeric value, a JS `number`. */
export function u16(value: number): Scalar {
  const arr = new Uint16Array([value]);
  return new Scalar(TypeU16, arr[0], arr);
}
/** Create a u8 from a numeric value, a JS `number`. */
export function u8(value: number): Scalar {
  const arr = new Uint8Array([value]);
  return new Scalar(TypeU8, arr[0], arr);
}

/** Create an u32 from a bit representation, a uint32 represented as a JS `number`. */
export function u32Bits(bits: number): Scalar {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeU32, bits, arr);
}
/** Create an u16 from a bit representation, a uint16 represented as a JS `number`. */
export function u16Bits(bits: number): Scalar {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeU16, bits, arr);
}
/** Create an u8 from a bit representation, a uint8 represented as a JS `number`. */
export function u8Bits(bits: number): Scalar {
  const arr = new Uint8Array([bits]);
  return new Scalar(TypeU8, bits, arr);
}

/** Create a boolean value. */
export function bool(value: boolean): Scalar {
  // WGSL does not support using 'bool' types directly in storage / uniform
  // buffers, so instead we pack booleans in a u32, where 'false' is zero and
  // 'true' is any non-zero value.
  const arr = new Uint32Array([value ? 1 : 0]);
  return new Scalar(TypeBool, value, arr);
}

/** A 'true' literal value */
export const True = bool(true);

/** A 'false' literal value */
export const False = bool(false);

// Encoding to u32s, instead of BigInt, for serialization
export function reinterpretF64AsU32s(f64: number): [number, number] {
  const array = new Float64Array(1);
  array[0] = f64;
  const u32s = new Uint32Array(array.buffer);
  return [u32s[0], u32s[1]];
}

// De-encoding from u32s, instead of BigInt, for serialization
export function reinterpretU32sAsF64(u32s: [number, number]): number {
  const array = new Uint32Array(2);
  array[0] = u32s[0];
  array[1] = u32s[1];
  return new Float64Array(array.buffer)[0];
}

/**
 * @returns a number representing the u32 interpretation
 * of the bits of a number assumed to be an f32 value.
 */
export function reinterpretF32AsU32(f32: number): number {
  const array = new Float32Array(1);
  array[0] = f32;
  return new Uint32Array(array.buffer)[0];
}

/**
 * @returns a number representing the i32 interpretation
 * of the bits of a number assumed to be an f32 value.
 */
export function reinterpretF32AsI32(f32: number): number {
  const array = new Float32Array(1);
  array[0] = f32;
  return new Int32Array(array.buffer)[0];
}

/**
 * @returns a number representing the f32 interpretation
 * of the bits of a number assumed to be an u32 value.
 */
export function reinterpretU32AsF32(u32: number): number {
  const array = new Uint32Array(1);
  array[0] = u32;
  return new Float32Array(array.buffer)[0];
}

/**
 * @returns a number representing the i32 interpretation
 * of the bits of a number assumed to be an u32 value.
 */
export function reinterpretU32AsI32(u32: number): number {
  const array = new Uint32Array(1);
  array[0] = u32;
  return new Int32Array(array.buffer)[0];
}

/**
 * @returns a number representing the u32 interpretation
 * of the bits of a number assumed to be an i32 value.
 */
export function reinterpretI32AsU32(i32: number): number {
  const array = new Int32Array(1);
  array[0] = i32;
  return new Uint32Array(array.buffer)[0];
}

/**
 * @returns a number representing the f32 interpretation
 * of the bits of a number assumed to be an i32 value.
 */
export function reinterpretI32AsF32(i32: number): number {
  const array = new Int32Array(1);
  array[0] = i32;
  return new Float32Array(array.buffer)[0];
}

/**
 * @returns a number representing the u16 interpretation
 * of the bits of a number assumed to be an f16 value.
 */
export function reinterpretF16AsU16(f16: number): number {
  const array = new Float16Array(1);
  array[0] = f16;
  return new Uint16Array(array.buffer)[0];
}

/**
 * @returns a number representing the f16 interpretation
 * of the bits of a number assumed to be an u16 value.
 */
export function reinterpretU16AsF16(u16: number): number {
  const array = new Uint16Array(1);
  array[0] = u16;
  return new Float16Array(array.buffer)[0];
}

/**
 * Class that encapsulates a vector value.
 */
export class Vector {
  readonly elements: Array<Scalar>;
  readonly type: VectorType;

  public constructor(elements: Array<Scalar>) {
    if (elements.length < 2 || elements.length > 4) {
      throw new Error(`vector element count must be between 2 and 4, got ${elements.length}`);
    }
    for (let i = 1; i < elements.length; i++) {
      const a = elements[0].type;
      const b = elements[i].type;
      if (a !== b) {
        throw new Error(
          `cannot mix vector element types. Found elements with types '${a}' and '${b}'`
        );
      }
    }
    this.elements = elements;
    this.type = TypeVec(elements.length, elements[0].type);
  }

  /**
   * Copies the vector value to the Uint8Array buffer at the provided byte offset.
   * @param buffer the destination buffer
   * @param offset the byte offset within buffer
   */
  public copyTo(buffer: Uint8Array, offset: number) {
    for (const element of this.elements) {
      element.copyTo(buffer, offset);
      offset += this.type.elementType.size;
    }
  }

  /**
   * @returns the WGSL representation of this vector value
   */
  public wgsl(): string {
    const els = this.elements.map(v => v.wgsl()).join(', ');
    return `vec${this.type.width}(${els})`;
  }

  public toString(): string {
    return `${this.type}(${this.elements.map(e => e.toString()).join(', ')})`;
  }

  public get x() {
    assert(0 < this.elements.length);
    return this.elements[0];
  }

  public get y() {
    assert(1 < this.elements.length);
    return this.elements[1];
  }

  public get z() {
    assert(2 < this.elements.length);
    return this.elements[2];
  }

  public get w() {
    assert(3 < this.elements.length);
    return this.elements[3];
  }
}

/** Helper for constructing a new two-element vector with the provided values */
export function vec2(x: Scalar, y: Scalar) {
  return new Vector([x, y]);
}

/** Helper for constructing a new three-element vector with the provided values */
export function vec3(x: Scalar, y: Scalar, z: Scalar) {
  return new Vector([x, y, z]);
}

/** Helper for constructing a new four-element vector with the provided values */
export function vec4(x: Scalar, y: Scalar, z: Scalar, w: Scalar) {
  return new Vector([x, y, z, w]);
}

/**
 * Helper for constructing Vectors from arrays of numbers
 *
 * @param v array of numbers to be converted, must contain 2, 3 or 4 elements
 * @param op function to convert from number to Scalar, e.g. 'f32`
 */
export function toVector(v: number[], op: (n: number) => Scalar): Vector {
  switch (v.length) {
    case 2:
      return vec2(op(v[0]), op(v[1]));
    case 3:
      return vec3(op(v[0]), op(v[1]), op(v[2]));
    case 4:
      return vec4(op(v[0]), op(v[1]), op(v[2]), op(v[3]));
  }
  unreachable(`input to 'toVector' must contain 2, 3, or 4 elements`);
}

/**
 * Class that encapsulates a Matrix value.
 */
export class Matrix {
  readonly elements: Scalar[][];
  readonly type: MatrixType;

  public constructor(elements: Array<Array<Scalar>>) {
    const num_cols = elements.length;
    if (num_cols < 2 || num_cols > 4) {
      throw new Error(`matrix cols count must be between 2 and 4, got ${num_cols}`);
    }

    const num_rows = elements[0].length;
    if (!elements.every(c => c.length === num_rows)) {
      throw new Error(`cannot mix matrix column lengths`);
    }

    if (num_rows < 2 || num_rows > 4) {
      throw new Error(`matrix rows count must be between 2 and 4, got ${num_rows}`);
    }

    const elem_type = elements[0][0].type;
    if (!elements.every(c => c.every(r => objectEquals(r.type, elem_type)))) {
      throw new Error(`cannot mix matrix element types`);
    }

    this.elements = elements;
    this.type = TypeMat(num_cols, num_rows, elem_type);
  }

  /**
   * Copies the matrix value to the Uint8Array buffer at the provided byte offset.
   * @param buffer the destination buffer
   * @param offset the byte offset within buffer
   */
  public copyTo(buffer: Uint8Array, offset: number) {
    for (let i = 0; i < this.type.cols; i++) {
      for (let j = 0; j < this.type.rows; j++) {
        this.elements[i][j].copyTo(buffer, offset);
        offset += this.type.elementType.size;
      }

      // vec3 have one padding element, so need to skip in matrices
      if (this.type.rows === 3) {
        offset += this.type.elementType.size;
      }
    }
  }

  /**
   * @returns the WGSL representation of this matrix value
   */
  public wgsl(): string {
    const els = this.elements.flatMap(c => c.map(r => r.wgsl())).join(', ');
    return `mat${this.type.cols}x${this.type.rows}(${els})`;
  }

  public toString(): string {
    return `${this.type}(${this.elements.map(c => c.join(', ')).join(', ')})`;
  }
}

/**
 * Helper for constructing Matrices from arrays of numbers
 *
 * @param m array of array of numbers to be converted, all Array of number must
 *          be of the same length. All Arrays must have 2, 3, or 4 elements.
 * @param op function to convert from number to Scalar, e.g. 'f32`
 */
export function toMatrix(m: number[][], op: (n: number) => Scalar): Matrix {
  const cols = m.length;
  const rows = m[0].length;
  const elements: Scalar[][] = [...Array<Scalar[]>(cols)].map(_ => [...Array<Scalar>(rows)]);
  for (let i = 0; i < cols; i++) {
    for (let j = 0; j < rows; j++) {
      elements[i][j] = op(m[i][j]);
    }
  }

  return new Matrix(elements);
}

/** Value is a Scalar or Vector value. */
export type Value = Scalar | Vector | Matrix;

export type SerializedValueScalar = {
  kind: 'scalar';
  type: ScalarKind;
  value: boolean | number;
};

export type SerializedValueVector = {
  kind: 'vector';
  type: ScalarKind;
  value: boolean[] | number[];
};

export type SerializedValueMatrix = {
  kind: 'matrix';
  type: ScalarKind;
  value: number[][];
};

export type SerializedValue = SerializedValueScalar | SerializedValueVector | SerializedValueMatrix;

export function serializeValue(v: Value): SerializedValue {
  const value = (kind: ScalarKind, s: Scalar) => {
    switch (kind) {
      case 'f32':
        return new Uint32Array(s.bits.buffer)[0];
      case 'f16':
        return new Uint16Array(s.bits.buffer)[0];
      default:
        return s.value;
    }
  };
  if (v instanceof Scalar) {
    const kind = v.type.kind;
    return {
      kind: 'scalar',
      type: kind,
      value: value(kind, v),
    };
  }
  if (v instanceof Vector) {
    const kind = v.type.elementType.kind;
    return {
      kind: 'vector',
      type: kind,
      value: v.elements.map(e => value(kind, e)) as boolean[] | number[],
    };
  }
  if (v instanceof Matrix) {
    const kind = v.type.elementType.kind;
    return {
      kind: 'matrix',
      type: kind,
      value: v.elements.map(c => c.map(r => value(kind, r))) as number[][],
    };
  }

  unreachable(`unhandled value type: ${v}`);
}

export function deserializeValue(data: SerializedValue): Value {
  const buildScalar = (v: ScalarValue): Scalar => {
    switch (data.type) {
      case 'abstract-float':
        return abstractFloat(v as number);
      case 'f64':
        return f64(v as number);
      case 'i32':
        return i32(v as number);
      case 'u32':
        return u32(v as number);
      case 'f32':
        return f32Bits(v as number);
      case 'i16':
        return i16(v as number);
      case 'u16':
        return u16(v as number);
      case 'f16':
        return f16Bits(v as number);
      case 'i8':
        return i8(v as number);
      case 'u8':
        return u8(v as number);
      case 'bool':
        return bool(v as boolean);
      default:
        unreachable(`unhandled value type: ${data.type}`);
    }
  };
  switch (data.kind) {
    case 'scalar': {
      return buildScalar(data.value);
    }
    case 'vector': {
      return new Vector(data.value.map(v => buildScalar(v)));
    }
    case 'matrix': {
      return new Matrix(data.value.map(c => c.map(buildScalar)));
    }
  }
}

/** @returns if the Value is a float scalar type */
export function isFloatValue(v: Value): boolean {
  return isFloatType(v.type);
}

/**
 * @returns if @p ty is an abstract numeric type.
 * @note this does not consider composite types.
 * Use elementType() if you want to test the element type.
 */
export function isAbstractType(ty: Type): boolean {
  if (ty instanceof ScalarType) {
    return ty.kind === 'abstract-float';
  }
  return false;
}

/**
 * @returns if @p ty is a floating point type.
 * @note this does not consider composite types.
 * Use elementType() if you want to test the element type.
 */
export function isFloatType(ty: Type): boolean {
  if (ty instanceof ScalarType) {
    return (
      ty.kind === 'abstract-float' || ty.kind === 'f64' || ty.kind === 'f32' || ty.kind === 'f16'
    );
  }
  return false;
}

/// All floating-point scalar types
export const kAllFloatScalars = [TypeAbstractFloat, TypeF32, TypeF16] as const;

/// All floating-point vec2 types
export const kAllFloatVector2 = [
  TypeVec(2, TypeAbstractFloat),
  TypeVec(2, TypeF32),
  TypeVec(2, TypeF16),
] as const;

/// All floating-point vec3 types
export const kAllFloatVector3 = [
  TypeVec(3, TypeAbstractFloat),
  TypeVec(3, TypeF32),
  TypeVec(3, TypeF16),
] as const;

/// All floating-point vec4 types
export const kAllFloatVector4 = [
  TypeVec(4, TypeAbstractFloat),
  TypeVec(4, TypeF32),
  TypeVec(4, TypeF16),
] as const;

/// All floating-point vector types
export const kAllFloatVectors = [
  ...kAllFloatVector2,
  ...kAllFloatVector3,
  ...kAllFloatVector4,
] as const;

/// All floating-point scalar and vector types
export const kAllFloatScalarsAndVectors = [...kAllFloatScalars, ...kAllFloatVectors] as const;

/// All integer scalar and vector types
export const kAllIntegerScalarsAndVectors = [
  TypeI32,
  TypeVec(2, TypeI32),
  TypeVec(3, TypeI32),
  TypeVec(4, TypeI32),
  TypeU32,
  TypeVec(2, TypeU32),
  TypeVec(3, TypeU32),
  TypeVec(4, TypeU32),
] as const;

/// All signed integer scalar and vector types
export const kAllSignedIntegerScalarsAndVectors = [
  TypeI32,
  TypeVec(2, TypeI32),
  TypeVec(3, TypeI32),
  TypeVec(4, TypeI32),
] as const;

/// All unsigned integer scalar and vector types
export const kAllUnsignedIntegerScalarsAndVectors = [
  TypeU32,
  TypeVec(2, TypeU32),
  TypeVec(3, TypeU32),
  TypeVec(4, TypeU32),
] as const;

/// All floating-point and integer scalar and vector types
export const kAllFloatAndIntegerScalarsAndVectors = [
  ...kAllFloatScalarsAndVectors,
  ...kAllIntegerScalarsAndVectors,
] as const;

/// All floating-point and signed integer scalar and vector types
export const kAllFloatAndSignedIntegerScalarsAndVectors = [
  ...kAllFloatScalarsAndVectors,
  ...kAllSignedIntegerScalarsAndVectors,
] as const;

/** @returns the inner element type of the given type */
export function elementType(t: ScalarType | VectorType | MatrixType) {
  if (t instanceof ScalarType) {
    return t;
  }
  return t.elementType;
}
