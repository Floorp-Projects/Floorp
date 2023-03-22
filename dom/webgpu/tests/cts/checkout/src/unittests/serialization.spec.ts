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
  SerializedComparator,
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
  u16,
  u32,
  u8,
  vec2,
  vec3,
  vec4,
} from '../webgpu/util/conversion.js';
import {
  deserializeF32Interval,
  serializeF32Interval,
  toF32Interval,
} from '../webgpu/util/f32_interval.js';

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
  ]) {
    const serialized = serializeValue(value);
    const deserialized = deserializeValue(serialized);
    t.expect(
      objectEquals(value, deserialized),
      `value ${value} -> serialize -> deserialize -> ${deserialized}`
    );
  }
});

g.test('f32_interval').fn(t => {
  for (const interval of [
    toF32Interval(0),
    toF32Interval(-0),
    toF32Interval(1),
    toF32Interval(-1),
    toF32Interval(0.5),
    toF32Interval(-0.5),
    toF32Interval(kValue.f32.positive.max),
    toF32Interval(kValue.f32.positive.min),
    toF32Interval(kValue.f32.subnormal.positive.max),
    toF32Interval(kValue.f32.subnormal.positive.min),
    toF32Interval(kValue.f32.subnormal.negative.max),
    toF32Interval(kValue.f32.subnormal.negative.min),
    toF32Interval(kValue.f32.infinity.positive),
    toF32Interval(kValue.f32.infinity.negative),

    toF32Interval([-0, 0]),
    toF32Interval([-1, 1]),
    toF32Interval([-0.5, 0.5]),
    toF32Interval([kValue.f32.positive.min, kValue.f32.positive.max]),
    toF32Interval([kValue.f32.subnormal.positive.min, kValue.f32.subnormal.positive.max]),
    toF32Interval([kValue.f32.subnormal.negative.min, kValue.f32.subnormal.negative.max]),
    toF32Interval([kValue.f32.infinity.negative, kValue.f32.infinity.positive]),
  ]) {
    const serialized = serializeF32Interval(interval);
    const deserialized = deserializeF32Interval(serialized);
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
    toF32Interval([-0.5, 0.5]),
    toF32Interval([kValue.f32.positive.min, kValue.f32.positive.max]),
    // Intervals
    [toF32Interval([-8.0, 0.5]), toF32Interval([2.0, 4.0])],
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
      const serialized = c.comparator as SerializedComparator;
      const deserialized = deserializeComparator(serialized);
      for (const val of c.testCases) {
        const got = deserialized(val);
        const expect = c.comparator(val);
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
      const serialized = c.comparator as SerializedComparator;
      const deserialized = deserializeComparator(serialized);
      for (const val of c.testCases) {
        const got = deserialized(val);
        const expect = c.comparator(val);
        t.expect(
          got.matched === expect.matched,
          `comparator(${val}): got: ${expect.matched}, expect: ${got.matched}`
        );
      }
    }
  });
});
