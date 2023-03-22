export const description = `Unit tests for conversion`;

import { makeTestGroup } from '../common/internal/test_group.js';
import { objectEquals } from '../common/util/util.js';
import { kValue } from '../webgpu/util/constants.js';
import {
  bool,
  f16Bits,
  f32,
  f32Bits,
  float16BitsToFloat32,
  float32ToFloat16Bits,
  float32ToFloatBits,
  floatBitsToNormalULPFromZero,
  floatBitsToNumber,
  i32,
  kFloat16Format,
  kFloat32Format,
  pack2x16float,
  pack2x16snorm,
  pack2x16unorm,
  pack4x8snorm,
  pack4x8unorm,
  Scalar,
  u32,
  vec2,
  vec3,
  vec4,
  Vector,
} from '../webgpu/util/conversion.js';

import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(UnitTest);

const cases = [
  [0b0_01111_0000000000, 1],
  [0b0_00001_0000000000, 0.00006103515625],
  [0b0_01101_0101010101, 0.33325195],
  [0b0_11110_1111111111, 65504],
  [0b0_00000_0000000000, 0],
  [0b0_01110_0000000000, 0.5],
  [0b0_01100_1001100110, 0.1999512],
  [0b0_01111_0000000001, 1.00097656],
  [0b0_10101_1001000000, 100],
  [0b1_01100_1001100110, -0.1999512],
  [0b1_10101_1001000000, -100],
];

g.test('float16BitsToFloat32').fn(t => {
  for (const [bits, number] of [
    ...cases,
    [0b1_00000_0000000000, -0], // (resulting sign is not actually tested)
    [0b0_00000_1111111111, 0.00006104], // subnormal f16 input
    [0b1_00000_1111111111, -0.00006104],
  ]) {
    const actual = float16BitsToFloat32(bits);
    t.expect(
      // some loose check
      Math.abs(actual - number) <= 0.00001,
      `for ${bits.toString(2)}, expected ${number}, got ${actual}`
    );
  }
});

g.test('float32ToFloat16Bits').fn(t => {
  for (const [bits, number] of [
    ...cases,
    [0b0_00000_0000000000, 0.00001], // input that becomes subnormal in f16 is rounded to 0
    [0b1_00000_0000000000, -0.00001], // and sign is preserved
  ]) {
    // some loose check
    const actual = float32ToFloat16Bits(number);
    t.expect(
      Math.abs(actual - bits) <= 1,
      `for ${number}, expected ${bits.toString(2)}, got ${actual.toString(2)}`
    );
  }
});

g.test('float32ToFloatBits_floatBitsToNumber')
  .paramsSubcasesOnly(u =>
    u
      .combine('signed', [0, 1] as const)
      .combine('exponentBits', [5, 8])
      .combine('mantissaBits', [10, 23])
  )
  .fn(t => {
    const { signed, exponentBits, mantissaBits } = t.params;
    const bias = (1 << (exponentBits - 1)) - 1;

    for (const [, value] of cases) {
      if (value < 0 && signed === 0) continue;
      const bits = float32ToFloatBits(value, signed, exponentBits, mantissaBits, bias);
      const reconstituted = floatBitsToNumber(bits, { signed, exponentBits, mantissaBits, bias });
      t.expect(Math.abs(reconstituted - value) <= 0.0000001, `${reconstituted} vs ${value}`);
    }
  });

g.test('floatBitsToULPFromZero,16').fn(t => {
  const test = (bits: number, ulpFromZero: number) =>
    t.expect(floatBitsToNormalULPFromZero(bits, kFloat16Format) === ulpFromZero, bits.toString(2));
  // Zero
  test(0b0_00000_0000000000, 0);
  // Subnormal
  test(0b0_00000_0000000001, 0);
  test(0b1_00000_0000000001, 0);
  test(0b0_00000_1111111111, 0);
  test(0b1_00000_1111111111, 0);
  // Normal
  test(0b0_00001_0000000000, 1); // 0 + 1ULP
  test(0b1_00001_0000000000, -1); // 0 - 1ULP
  test(0b0_00001_0000000001, 2); // 0 + 2ULP
  test(0b1_00001_0000000001, -2); // 0 - 2ULP
  test(0b0_01110_0000000000, 0b01101_0000000001); // 0.5
  test(0b1_01110_0000000000, -0b01101_0000000001); // -0.5
  test(0b0_01110_1111111110, 0b01101_1111111111); // 1.0 - 2ULP
  test(0b1_01110_1111111110, -0b01101_1111111111); // -(1.0 - 2ULP)
  test(0b0_01110_1111111111, 0b01110_0000000000); // 1.0 - 1ULP
  test(0b1_01110_1111111111, -0b01110_0000000000); // -(1.0 - 1ULP)
  test(0b0_01111_0000000000, 0b01110_0000000001); // 1.0
  test(0b1_01111_0000000000, -0b01110_0000000001); // -1.0
  test(0b0_01111_0000000001, 0b01110_0000000010); // 1.0 + 1ULP
  test(0b1_01111_0000000001, -0b01110_0000000010); // -(1.0 + 1ULP)
  test(0b0_10000_0000000000, 0b01111_0000000001); // 2.0
  test(0b1_10000_0000000000, -0b01111_0000000001); // -2.0

  const testThrows = (b: number) =>
    t.shouldThrow('Error', () => floatBitsToNormalULPFromZero(b, kFloat16Format));
  // Infinity
  testThrows(0b0_11111_0000000000);
  testThrows(0b1_11111_0000000000);
  // NaN
  testThrows(0b0_11111_1111111111);
  testThrows(0b1_11111_1111111111);
});

g.test('floatBitsToULPFromZero,32').fn(t => {
  const test = (bits: number, ulpFromZero: number) =>
    t.expect(floatBitsToNormalULPFromZero(bits, kFloat32Format) === ulpFromZero, bits.toString(2));
  // Zero
  test(0b0_00000000_00000000000000000000000, 0);
  // Subnormal
  test(0b0_00000000_00000000000000000000001, 0);
  test(0b1_00000000_00000000000000000000001, 0);
  test(0b0_00000000_11111111111111111111111, 0);
  test(0b1_00000000_11111111111111111111111, 0);
  // Normal
  test(0b0_00000001_00000000000000000000000, 1); // 0 + 1ULP
  test(0b1_00000001_00000000000000000000000, -1); // 0 - 1ULP
  test(0b0_00000001_00000000000000000000001, 2); // 0 + 2ULP
  test(0b1_00000001_00000000000000000000001, -2); // 0 - 2ULP
  test(0b0_01111110_00000000000000000000000, 0b01111101_00000000000000000000001); // 0.5
  test(0b1_01111110_00000000000000000000000, -0b01111101_00000000000000000000001); // -0.5
  test(0b0_01111110_11111111111111111111110, 0b01111101_11111111111111111111111); // 1.0 - 2ULP
  test(0b1_01111110_11111111111111111111110, -0b01111101_11111111111111111111111); // -(1.0 - 2ULP)
  test(0b0_01111110_11111111111111111111111, 0b01111110_00000000000000000000000); // 1.0 - 1ULP
  test(0b1_01111110_11111111111111111111111, -0b01111110_00000000000000000000000); // -(1.0 - 1ULP)
  test(0b0_01111111_00000000000000000000000, 0b01111110_00000000000000000000001); // 1.0
  test(0b1_01111111_00000000000000000000000, -0b01111110_00000000000000000000001); // -1.0
  test(0b0_01111111_00000000000000000000001, 0b01111110_00000000000000000000010); // 1.0 + 1ULP
  test(0b1_01111111_00000000000000000000001, -0b01111110_00000000000000000000010); // -(1.0 + 1ULP)
  test(0b0_11110000_00000000000000000000000, 0b11101111_00000000000000000000001); // 2.0
  test(0b1_11110000_00000000000000000000000, -0b11101111_00000000000000000000001); // -2.0

  const testThrows = (b: number) =>
    t.shouldThrow('Error', () => floatBitsToNormalULPFromZero(b, kFloat32Format));
  // Infinity
  testThrows(0b0_11111111_00000000000000000000000);
  testThrows(0b1_11111111_00000000000000000000000);
  // NaN
  testThrows(0b0_11111111_11111111111111111111111);
  testThrows(0b0_11111111_00000000000000000000001);
  testThrows(0b1_11111111_11111111111111111111111);
  testThrows(0b1_11111111_00000000000000000000001);
});

g.test('scalarWGSL').fn(t => {
  const cases: Array<[Scalar, string]> = [
    [f32(0.0), '0.0f'],
    [f32(1.0), '1.0f'],
    [f32(-1.0), '-1.0f'],
    [f32Bits(0x70000000), '1.5845632502852868e+29f'],
    [f32Bits(0xf0000000), '-1.5845632502852868e+29f'],
    [f16Bits(0), '0.0h'],
    [f16Bits(0x3c00), '1.0h'],
    [f16Bits(0xbc00), '-1.0h'],
    [u32(0), '0u'],
    [u32(1), '1u'],
    [u32(2000000000), '2000000000u'],
    [u32(-1), '4294967295u'],
    [i32(0), 'i32(0)'],
    [i32(1), 'i32(1)'],
    [i32(-1), 'i32(-1)'],
    [bool(true), 'true'],
    [bool(false), 'false'],
  ];
  for (const [value, expect] of cases) {
    const got = value.wgsl();
    t.expect(
      got === expect,
      `[value: ${value.value}, type: ${value.type}]
got:    ${got}
expect: ${expect}`
    );
  }
});

g.test('vectorWGSL').fn(t => {
  const cases: Array<[Vector, string]> = [
    [vec2(f32(42.0), f32(24.0)), 'vec2(42.0f, 24.0f)'],
    [vec2(f16Bits(0x5140), f16Bits(0x4e00)), 'vec2(42.0h, 24.0h)'],
    [vec2(u32(42), u32(24)), 'vec2(42u, 24u)'],
    [vec2(i32(42), i32(24)), 'vec2(i32(42), i32(24))'],
    [vec2(bool(false), bool(true)), 'vec2(false, true)'],

    [vec3(f32(0.0), f32(1.0), f32(-1.0)), 'vec3(0.0f, 1.0f, -1.0f)'],
    [vec3(f16Bits(0), f16Bits(0x3c00), f16Bits(0xbc00)), 'vec3(0.0h, 1.0h, -1.0h)'],
    [vec3(u32(0), u32(1), u32(-1)), 'vec3(0u, 1u, 4294967295u)'],
    [vec3(i32(0), i32(1), i32(-1)), 'vec3(i32(0), i32(1), i32(-1))'],
    [vec3(bool(true), bool(false), bool(true)), 'vec3(true, false, true)'],

    [vec4(f32(1.0), f32(-2.0), f32(4.0), f32(-8.0)), 'vec4(1.0f, -2.0f, 4.0f, -8.0f)'],
    [
      vec4(f16Bits(0xbc00), f16Bits(0x4000), f16Bits(0xc400), f16Bits(0x4800)),
      'vec4(-1.0h, 2.0h, -4.0h, 8.0h)',
    ],
    [vec4(u32(1), u32(-2), u32(4), u32(-8)), 'vec4(1u, 4294967294u, 4u, 4294967288u)'],
    [vec4(i32(1), i32(-2), i32(4), i32(-8)), 'vec4(i32(1), i32(-2), i32(4), i32(-8))'],
    [vec4(bool(false), bool(true), bool(true), bool(false)), 'vec4(false, true, true, false)'],
  ];
  for (const [value, expect] of cases) {
    const got = value.wgsl();
    t.expect(
      got === expect,
      `[values: ${value.elements}, type: ${value.type}]
got:    ${got}
expect: ${expect}`
    );
  }
});

g.test('pack2x16float')
  .paramsSimple([
    // f16 normals
    { inputs: [0, 0], result: [0x00000000, 0x80000000, 0x00008000, 0x80008000] },
    { inputs: [1, 0], result: [0x00003c00, 0x80003c00] },
    { inputs: [1, 1], result: [0x3c003c00] },
    { inputs: [-1, -1], result: [0xbc00bc00] },
    { inputs: [10, 1], result: [0x3c004900] },
    { inputs: [-10, 1], result: [0x3c00c900] },

    // f32 normal, but not f16 precise
    { inputs: [1.00000011920928955078125, 1], result: [0x3c003c00, 0x3c003c01] },

    // f32 subnormals
    // prettier-ignore
    { inputs: [kValue.f32.subnormal.positive.max, 1], result: [0x3c000000, 0x3c008000, 0x3c000001] },
    // prettier-ignore
    { inputs: [kValue.f32.subnormal.negative.min, 1], result: [0x3c008001, 0x3c000000, 0x3c008000] },

    // f16 subnormals
    // prettier-ignore
    { inputs: [kValue.f16.subnormal.positive.max, 1], result: [0x3c0003ff, 0x3c000000, 0x3c008000] },
    // prettier-ignore
    { inputs: [kValue.f16.subnormal.negative.min, 1], result: [0x03c0083ff, 0x3c000000, 0x3c008000] },

    // f16 out of bounds
    { inputs: [kValue.f16.positive.max + 1, 1], result: [undefined] },
    { inputs: [kValue.f16.negative.min - 1, 1], result: [undefined] },
    { inputs: [1, kValue.f16.positive.max + 1], result: [undefined] },
    { inputs: [1, kValue.f16.negative.min - 1], result: [undefined] },
  ] as const)
  .fn(test => {
    const toString = (data: readonly (undefined | number)[]): String[] => {
      return data.map(d => (d !== undefined ? u32(d).toString() : 'undefined'));
    };

    const inputs = test.params.inputs;
    const got = pack2x16float(inputs[0], inputs[1]);
    const expect = test.params.result;

    const got_str = toString(got);
    const expect_str = toString(expect);

    // Using strings of the outputs, so they can be easily sorted, since order of the results doesn't matter.
    test.expect(
      objectEquals(got_str.sort(), expect_str.sort()),
      `pack2x16float(${inputs}) returned [${got_str}]. Expected [${expect_str}]`
    );
  });

g.test('pack2x16snorm')
  .paramsSimple([
    // Normals
    { inputs: [0, 0], result: 0x00000000 },
    { inputs: [1, 0], result: 0x00007fff },
    { inputs: [0, 1], result: 0x7fff0000 },
    { inputs: [1, 1], result: 0x7fff7fff },
    { inputs: [-1, -1], result: 0x80018001 },
    { inputs: [10, 10], result: 0x7fff7fff },
    { inputs: [-10, -10], result: 0x80018001 },
    { inputs: [0.1, 0.1], result: 0x0ccd0ccd },
    { inputs: [-0.1, -0.1], result: 0xf333f333 },
    { inputs: [0.5, 0.5], result: 0x40004000 },
    { inputs: [-0.5, -0.5], result: 0xc001c001 },
    { inputs: [0.1, 0.5], result: 0x40000ccd },
    { inputs: [-0.1, -0.5], result: 0xc001f333 },

    // Subnormals
    { inputs: [kValue.f32.subnormal.positive.max, 1], result: 0x7fff0000 },
    { inputs: [kValue.f32.subnormal.negative.min, 1], result: 0x7fff0000 },
  ] as const)
  .fn(test => {
    const inputs = test.params.inputs;
    const got = pack2x16snorm(inputs[0], inputs[1]);
    const expect = test.params.result;

    test.expect(got === expect, `pack2x16snorm(${inputs}) returned ${got}. Expected ${expect}`);
  });

g.test('pack2x16unorm')
  .paramsSimple([
    // Normals
    { inputs: [0, 0], result: 0x00000000 },
    { inputs: [1, 0], result: 0x0000ffff },
    { inputs: [0, 1], result: 0xffff0000 },
    { inputs: [1, 1], result: 0xffffffff },
    { inputs: [-1, -1], result: 0x00000000 },
    { inputs: [0.1, 0.1], result: 0x199a199a },
    { inputs: [0.5, 0.5], result: 0x80008000 },
    { inputs: [0.1, 0.5], result: 0x8000199a },
    { inputs: [10, 10], result: 0xffffffff },

    // Subnormals
    { inputs: [kValue.f32.subnormal.positive.max, 1], result: 0xffff0000 },
  ] as const)
  .fn(test => {
    const inputs = test.params.inputs;
    const got = pack2x16unorm(inputs[0], inputs[1]);
    const expect = test.params.result;

    test.expect(got === expect, `pack2x16unorm(${inputs}) returned ${got}. Expected ${expect}`);
  });

g.test('pack4x8snorm')
  .paramsSimple([
    // Normals
    { inputs: [0, 0, 0, 0], result: 0x00000000 },
    { inputs: [1, 0, 0, 0], result: 0x0000007f },
    { inputs: [0, 1, 0, 0], result: 0x00007f00 },
    { inputs: [0, 0, 1, 0], result: 0x007f0000 },
    { inputs: [0, 0, 0, 1], result: 0x7f000000 },
    { inputs: [1, 1, 1, 1], result: 0x7f7f7f7f },
    { inputs: [10, 10, 10, 10], result: 0x7f7f7f7f },
    { inputs: [-1, 0, 0, 0], result: 0x00000081 },
    { inputs: [0, -1, 0, 0], result: 0x00008100 },
    { inputs: [0, 0, -1, 0], result: 0x00810000 },
    { inputs: [0, 0, 0, -1], result: 0x81000000 },
    { inputs: [-1, -1, -1, -1], result: 0x81818181 },
    { inputs: [-10, -10, -10, -10], result: 0x81818181 },
    { inputs: [0.1, 0.1, 0.1, 0.1], result: 0x0d0d0d0d },
    { inputs: [-0.1, -0.1, -0.1, -0.1], result: 0xf3f3f3f3 },
    { inputs: [0.1, -0.1, 0.1, -0.1], result: 0xf30df30d },
    { inputs: [0.5, 0.5, 0.5, 0.5], result: 0x40404040 },
    { inputs: [-0.5, -0.5, -0.5, -0.5], result: 0xc1c1c1c1 },
    { inputs: [-0.5, 0.5, -0.5, 0.5], result: 0x40c140c1 },
    { inputs: [0.1, 0.5, 0.1, 0.5], result: 0x400d400d },
    { inputs: [-0.1, -0.5, -0.1, -0.5], result: 0xc1f3c1f3 },

    // Subnormals
    { inputs: [kValue.f32.subnormal.positive.max, 1, 1, 1], result: 0x7f7f7f00 },
    { inputs: [kValue.f32.subnormal.negative.min, 1, 1, 1], result: 0x7f7f7f00 },
  ] as const)
  .fn(test => {
    const inputs = test.params.inputs;
    const got = pack4x8snorm(inputs[0], inputs[1], inputs[2], inputs[3]);
    const expect = test.params.result;

    test.expect(got === expect, `pack4x8snorm(${inputs}) returned ${u32(got)}. Expected ${expect}`);
  });

g.test('pack4x8unorm')
  .paramsSimple([
    // Normals
    { inputs: [0, 0, 0, 0], result: 0x00000000 },
    { inputs: [1, 0, 0, 0], result: 0x000000ff },
    { inputs: [0, 1, 0, 0], result: 0x0000ff00 },
    { inputs: [0, 0, 1, 0], result: 0x00ff0000 },
    { inputs: [0, 0, 0, 1], result: 0xff000000 },
    { inputs: [1, 1, 1, 1], result: 0xffffffff },
    { inputs: [10, 10, 10, 10], result: 0xffffffff },
    { inputs: [-1, -1, -1, -1], result: 0x00000000 },
    { inputs: [-10, -10, -10, -10], result: 0x00000000 },
    { inputs: [0.1, 0.1, 0.1, 0.1], result: 0x1a1a1a1a },
    { inputs: [0.5, 0.5, 0.5, 0.5], result: 0x80808080 },
    { inputs: [0.1, 0.5, 0.1, 0.5], result: 0x801a801a },

    // Subnormals
    { inputs: [kValue.f32.subnormal.positive.max, 1, 1, 1], result: 0xffffff00 },
  ] as const)
  .fn(test => {
    const inputs = test.params.inputs;
    const got = pack4x8unorm(inputs[0], inputs[1], inputs[2], inputs[3]);
    const expect = test.params.result;

    test.expect(got === expect, `pack4x8unorm(${inputs}) returned ${got}. Expected ${expect}`);
  });
