export const description = `Unit tests for data cache serialization`;

import { getIsBuildingDataCache, setIsBuildingDataCache } from '../common/framework/data_cache.js';
import { makeTestGroup } from '../common/internal/test_group.js';
import { objectEquals } from '../common/util/util.js';
import {
  deserializeExpectation,
  serializeExpectation,
} from '../webgpu/shader/execution/expression/case_cache.js';
import {
  anyOf,
  deserializeComparator,
  serializeComparator,
  skipUndefined,
} from '../webgpu/util/compare.js';
import { kValue } from '../webgpu/util/constants.js';
import {
  bool,
  deserializeValue,
  f16,
  f32,
  i16,
  i32,
  i8,
  serializeValue,
  toMatrix,
  u16,
  u32,
  u8,
  vec2,
  vec3,
  vec4,
} from '../webgpu/util/conversion.js';
import { deserializeFPInterval, FP, serializeFPInterval } from '../webgpu/util/floating_point.js';

import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(UnitTest);

g.test('value').fn(t => {
  for (const value of [
    u32(kValue.u32.min + 0),
    u32(kValue.u32.min + 1),
    u32(kValue.u32.min + 2),
    u32(kValue.u32.max - 2),
    u32(kValue.u32.max - 1),
    u32(kValue.u32.max - 0),

    u16(kValue.u16.min + 0),
    u16(kValue.u16.min + 1),
    u16(kValue.u16.min + 2),
    u16(kValue.u16.max - 2),
    u16(kValue.u16.max - 1),
    u16(kValue.u16.max - 0),

    u8(kValue.u8.min + 0),
    u8(kValue.u8.min + 1),
    u8(kValue.u8.min + 2),
    u8(kValue.u8.max - 2),
    u8(kValue.u8.max - 1),
    u8(kValue.u8.max - 0),

    i32(kValue.i32.negative.min + 0),
    i32(kValue.i32.negative.min + 1),
    i32(kValue.i32.negative.min + 2),
    i32(kValue.i32.negative.max - 2),
    i32(kValue.i32.negative.max - 1),
    i32(kValue.i32.positive.min - 0),
    i32(kValue.i32.positive.min + 1),
    i32(kValue.i32.positive.min + 2),
    i32(kValue.i32.positive.max - 2),
    i32(kValue.i32.positive.max - 1),
    i32(kValue.i32.positive.max - 0),

    i16(kValue.i16.negative.min + 0),
    i16(kValue.i16.negative.min + 1),
    i16(kValue.i16.negative.min + 2),
    i16(kValue.i16.negative.max - 2),
    i16(kValue.i16.negative.max - 1),
    i16(kValue.i16.positive.min + 0),
    i16(kValue.i16.positive.min + 1),
    i16(kValue.i16.positive.min + 2),
    i16(kValue.i16.positive.max - 2),
    i16(kValue.i16.positive.max - 1),
    i16(kValue.i16.positive.max - 0),

    i8(kValue.i8.negative.min + 0),
    i8(kValue.i8.negative.min + 1),
    i8(kValue.i8.negative.min + 2),
    i8(kValue.i8.negative.max - 2),
    i8(kValue.i8.negative.max - 1),
    i8(kValue.i8.positive.min + 0),
    i8(kValue.i8.positive.min + 1),
    i8(kValue.i8.positive.min + 2),
    i8(kValue.i8.positive.max - 2),
    i8(kValue.i8.positive.max - 1),
    i8(kValue.i8.positive.max - 0),

    f32(0),
    f32(-0),
    f32(1),
    f32(-1),
    f32(0.5),
    f32(-0.5),
    f32(kValue.f32.positive.max),
    f32(kValue.f32.positive.min),
    f32(kValue.f32.subnormal.positive.max),
    f32(kValue.f32.subnormal.positive.min),
    f32(kValue.f32.subnormal.negative.max),
    f32(kValue.f32.subnormal.negative.min),
    f32(kValue.f32.infinity.positive),
    f32(kValue.f32.infinity.negative),

    f16(0),
    f16(-0),
    f16(1),
    f16(-1),
    f16(0.5),
    f16(-0.5),
    f16(kValue.f32.positive.max),
    f16(kValue.f32.positive.min),
    f16(kValue.f32.subnormal.positive.max),
    f16(kValue.f32.subnormal.positive.min),
    f16(kValue.f32.subnormal.negative.max),
    f16(kValue.f32.subnormal.negative.min),
    f16(kValue.f32.infinity.positive),
    f16(kValue.f32.infinity.negative),

    bool(true),
    bool(false),

    vec2(f32(1), f32(2)),
    vec3(u32(1), u32(2), u32(3)),
    vec4(bool(false), bool(true), bool(false), bool(true)),

    toMatrix(
      [
        [0.0, 1.0],
        [2.0, 3.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0],
        [3.0, 4.0, 5.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0, 3.0],
        [4.0, 5.0, 6.0, 7.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0],
        [2.0, 3.0],
        [4.0, 5.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0],
        [3.0, 4.0, 5.0],
        [6.0, 7.0, 8.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0, 3.0],
        [4.0, 5.0, 6.0, 7.0],
        [8.0, 9.0, 10.0, 11.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0],
        [2.0, 3.0],
        [4.0, 5.0],
        [6.0, 7.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0],
        [3.0, 4.0, 5.0],
        [6.0, 7.0, 8.0],
        [9.0, 10.0, 11.0],
      ],
      f32
    ),
    toMatrix(
      [
        [0.0, 1.0, 2.0, 3.0],
        [4.0, 5.0, 6.0, 7.0],
        [8.0, 9.0, 10.0, 11.0],
        [12.0, 13.0, 14.0, 15.0],
      ],
      f32
    ),
  ]) {
    const serialized = serializeValue(value);
    const deserialized = deserializeValue(serialized);
    t.expect(
      objectEquals(value, deserialized),
      `value ${value} -> serialize -> deserialize -> ${deserialized}`
    );
  }
});

g.test('fpinterval_f32').fn(t => {
  for (const interval of [
    FP.f32.toInterval(0),
    FP.f32.toInterval(-0),
    FP.f32.toInterval(1),
    FP.f32.toInterval(-1),
    FP.f32.toInterval(0.5),
    FP.f32.toInterval(-0.5),
    FP.f32.toInterval(kValue.f32.positive.max),
    FP.f32.toInterval(kValue.f32.positive.min),
    FP.f32.toInterval(kValue.f32.subnormal.positive.max),
    FP.f32.toInterval(kValue.f32.subnormal.positive.min),
    FP.f32.toInterval(kValue.f32.subnormal.negative.max),
    FP.f32.toInterval(kValue.f32.subnormal.negative.min),
    FP.f32.toInterval(kValue.f32.infinity.positive),
    FP.f32.toInterval(kValue.f32.infinity.negative),

    FP.f32.toInterval([-0, 0]),
    FP.f32.toInterval([-1, 1]),
    FP.f32.toInterval([-0.5, 0.5]),
    FP.f32.toInterval([kValue.f32.positive.min, kValue.f32.positive.max]),
    FP.f32.toInterval([kValue.f32.subnormal.positive.min, kValue.f32.subnormal.positive.max]),
    FP.f32.toInterval([kValue.f32.subnormal.negative.min, kValue.f32.subnormal.negative.max]),
    FP.f32.toInterval([kValue.f32.infinity.negative, kValue.f32.infinity.positive]),
  ]) {
    const serialized = serializeFPInterval(interval);
    const deserialized = deserializeFPInterval(serialized);
    t.expect(
      objectEquals(interval, deserialized),
      `interval ${interval} -> serialize -> deserialize -> ${deserialized}`
    );
  }
});

g.test('fpinterval_abstract').fn(t => {
  for (const interval of [
    FP.abstract.toInterval(0),
    FP.abstract.toInterval(-0),
    FP.abstract.toInterval(1),
    FP.abstract.toInterval(-1),
    FP.abstract.toInterval(0.5),
    FP.abstract.toInterval(-0.5),
    FP.abstract.toInterval(kValue.f64.positive.max),
    FP.abstract.toInterval(kValue.f64.positive.min),
    FP.abstract.toInterval(kValue.f64.subnormal.positive.max),
    FP.abstract.toInterval(kValue.f64.subnormal.positive.min),
    FP.abstract.toInterval(kValue.f64.subnormal.negative.max),
    FP.abstract.toInterval(kValue.f64.subnormal.negative.min),
    FP.abstract.toInterval(kValue.f64.infinity.positive),
    FP.abstract.toInterval(kValue.f64.infinity.negative),

    FP.abstract.toInterval([-0, 0]),
    FP.abstract.toInterval([-1, 1]),
    FP.abstract.toInterval([-0.5, 0.5]),
    FP.abstract.toInterval([kValue.f64.positive.min, kValue.f64.positive.max]),
    FP.abstract.toInterval([kValue.f64.subnormal.positive.min, kValue.f64.subnormal.positive.max]),
    FP.abstract.toInterval([kValue.f64.subnormal.negative.min, kValue.f64.subnormal.negative.max]),
    FP.abstract.toInterval([kValue.f64.infinity.negative, kValue.f64.infinity.positive]),
  ]) {
    const serialized = serializeFPInterval(interval);
    const deserialized = deserializeFPInterval(serialized);
    t.expect(
      objectEquals(interval, deserialized),
      `interval ${interval} -> serialize -> deserialize -> ${deserialized}`
    );
  }
});

g.test('expression_expectation').fn(t => {
  for (const expectation of [
    // Value
    f32(123),
    vec2(f32(1), f32(2)),
    // Interval
    FP.f32.toInterval([-0.5, 0.5]),
    FP.f32.toInterval([kValue.f32.positive.min, kValue.f32.positive.max]),
    // Intervals
    [FP.f32.toInterval([-8.0, 0.5]), FP.f32.toInterval([2.0, 4.0])],
  ]) {
    const serialized = serializeExpectation(expectation);
    const deserialized = deserializeExpectation(serialized);
    t.expect(
      objectEquals(expectation, deserialized),
      `expectation ${expectation} -> serialize -> deserialize -> ${deserialized}`
    );
  }
});

/**
 * Temporarily enabled building of the data cache.
 * Required for Comparators to serialize.
 */
function enableBuildingDataCache(f: () => void) {
  const wasBuildingDataCache = getIsBuildingDataCache();
  setIsBuildingDataCache(true);
  f();
  setIsBuildingDataCache(wasBuildingDataCache);
}

g.test('anyOf').fn(t => {
  enableBuildingDataCache(() => {
    for (const c of [
      {
        comparator: anyOf(i32(123)),
        testCases: [f32(0), f32(10), f32(122), f32(123), f32(124), f32(200)],
      },
    ]) {
      const serialized = serializeComparator(c.comparator);
      const deserialized = deserializeComparator(serialized);
      for (const val of c.testCases) {
        const got = deserialized.compare(val);
        const expect = c.comparator.compare(val);
        t.expect(
          got.matched === expect.matched,
          `comparator(${val}): got: ${expect.matched}, expect: ${got.matched}`
        );
      }
    }
  });
});

g.test('skipUndefined').fn(t => {
  enableBuildingDataCache(() => {
    for (const c of [
      {
        comparator: skipUndefined(i32(123)),
        testCases: [f32(0), f32(10), f32(122), f32(123), f32(124), f32(200)],
      },
      {
        comparator: skipUndefined(undefined),
        testCases: [f32(0), f32(10), f32(122), f32(123), f32(124), f32(200)],
      },
    ]) {
      const serialized = serializeComparator(c.comparator);
      const deserialized = deserializeComparator(serialized);
      for (const val of c.testCases) {
        const got = deserialized.compare(val);
        const expect = c.comparator.compare(val);
        t.expect(
          got.matched === expect.matched,
          `comparator(${val}): got: ${expect.matched}, expect: ${got.matched}`
        );
      }
    }
  });
});
