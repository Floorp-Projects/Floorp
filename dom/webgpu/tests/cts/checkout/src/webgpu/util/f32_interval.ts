import { assert, unreachable } from '../../common/util/util.js';
import { Float16Array } from '../../external/petamoriken/float16/float16.js';

import { kValue } from './constants.js';
import { reinterpretF32AsU32, reinterpretU32AsF32 } from './conversion.js';
import {
  calculatePermutations,
  cartesianProduct,
  correctlyRoundedF16,
  correctlyRoundedF32,
  flushSubnormalNumberF32,
  isFiniteF16,
  isFiniteF32,
  isSubnormalNumberF16,
  isSubnormalNumberF32,
  oneULP,
} from './math.js';

/**
 * Representation of bounds for an interval as an array with either one or two
 * elements. Single element indicates that the interval is a single point. For
 * two elements, the first is the lower bound of the interval and the second is
 * the upper bound.
 */
export type IntervalBounds = [number] | [number, number];

/** Represents a closed interval in the f32 range */
export class F32Interval {
  public readonly begin: number;
  public readonly end: number;
  private static _any: F32Interval;

  /** Constructor
   *
   * `toF32Interval` is the preferred way to create F32Intervals
   *
   * @param bounds either a pair of numbers indicating the beginning then the
   *               end of the interval, or a single element array indicating the
   *               interval is a point
   */
  public constructor(...bounds: IntervalBounds) {
    const [begin, end] = bounds.length === 2 ? bounds : [bounds[0], bounds[0]];
    assert(!Number.isNaN(begin) && !Number.isNaN(end), `bounds need to be non-NaN`);
    assert(begin <= end, `bounds[0] (${begin}) must be less than or equal to bounds[1]  (${end})`);

    this.begin = begin;
    this.end = end;
  }

  /** @returns begin and end if non-point interval, otherwise just begin */
  public bounds(): IntervalBounds {
    return this.isPoint() ? [this.begin] : [this.begin, this.end];
  }

  /** @returns if a point or interval is completely contained by this interval */
  public contains(n: number | F32Interval): boolean {
    if (Number.isNaN(n)) {
      // Being the any interval indicates that accuracy is not defined for this
      // test, so the test is just checking that this input doesn't cause the
      // implementation to misbehave, so NaN is accepted.
      return this.begin === Number.NEGATIVE_INFINITY && this.end === Number.POSITIVE_INFINITY;
    }
    const i = toF32Interval(n);
    return this.begin <= i.begin && this.end >= i.end;
  }

  /** @returns if any values in the interval may be flushed to zero, this
   *           includes any subnormals and zero itself.
   */
  public containsZeroOrSubnormals(): boolean {
    return !(
      this.end < kValue.f32.subnormal.negative.min || this.begin > kValue.f32.subnormal.positive.max
    );
  }

  /** @returns if this interval contains a single point */
  public isPoint(): boolean {
    return this.begin === this.end;
  }

  /** @returns if this interval only contains f32 finite values */
  public isFinite(): boolean {
    return isFiniteF32(this.begin) && isFiniteF32(this.end);
  }

  /** @returns an interval with the tightest bounds that includes all provided intervals */
  static span(...intervals: F32Interval[]): F32Interval {
    assert(intervals.length > 0, `span of an empty list of F32Intervals is not allowed`);
    let begin = Number.POSITIVE_INFINITY;
    let end = Number.NEGATIVE_INFINITY;
    intervals.forEach(i => {
      begin = Math.min(i.begin, begin);
      end = Math.max(i.end, end);
    });
    return new F32Interval(begin, end);
  }

  /** @returns a string representation for logging purposes */
  public toString(): string {
    return `[${this.bounds()}]`;
  }

  /** @returns a singleton for interval of all possible values
   * This interval is used in situations where accuracy is not defined, so any
   * result is valid.
   */
  public static any(): F32Interval {
    if (this._any === undefined) {
      this._any = new F32Interval(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY);
    }
    return this._any;
  }
}

/**
 * SerializedF32Interval holds the serialized form of a F32Interval.
 * This form can be safely encoded to JSON.
 */
export type SerializedF32Interval = { begin: number; end: number } | 'any';

/** serializeF32Interval() converts a F32Interval to a SerializedF32Interval */
export function serializeF32Interval(i: F32Interval): SerializedF32Interval {
  return i === F32Interval.any()
    ? 'any'
    : { begin: reinterpretF32AsU32(i.begin), end: reinterpretF32AsU32(i.end) };
}

/** serializeF32Interval() converts a SerializedF32Interval to a F32Interval */
export function deserializeF32Interval(data: SerializedF32Interval): F32Interval {
  return data === 'any'
    ? F32Interval.any()
    : toF32Interval([reinterpretU32AsF32(data.begin), reinterpretU32AsF32(data.end)]);
}

/** @returns an interval containing the point or the original interval */
export function toF32Interval(n: number | IntervalBounds | F32Interval): F32Interval {
  if (n instanceof F32Interval) {
    return n;
  }

  if (n instanceof Array) {
    return new F32Interval(...n);
  }

  return new F32Interval(n, n);
}

/** F32Interval of [-π, π] */
const kNegPiToPiInterval = toF32Interval([
  kValue.f32.negative.pi.whole,
  kValue.f32.positive.pi.whole,
]);

/** F32Interval of values greater than 0 and less than or equal to f32 max */
const kGreaterThanZeroInterval = toF32Interval([
  kValue.f32.subnormal.positive.min,
  kValue.f32.positive.max,
]);

/** Representation of a vec2/3/4 of floating point intervals as an array of F32Intervals */
export type F32Vector =
  | [F32Interval, F32Interval]
  | [F32Interval, F32Interval, F32Interval]
  | [F32Interval, F32Interval, F32Interval, F32Interval];

/** Coerce F32Interval[] to F32Vector if possible */
function isF32Vector(v: number[] | IntervalBounds[] | F32Interval[] | F32Vector): v is F32Vector {
  if (v[0] instanceof F32Interval) {
    return v.length === 2 || v.length === 3 || v.length === 4;
  }
  return false;
}

/** @returns an F32Vector representation of an array fo F32Intervals if possible */
export function toF32Vector(v: number[] | IntervalBounds[] | F32Interval[] | F32Vector): F32Vector {
  if (isF32Vector(v)) {
    return v;
  }

  const f = v.map(toF32Interval);
  if (isF32Vector(f)) {
    return f;
  }
  unreachable(`Cannot convert [${v}] to F32Vector`);
}

/** F32Vector with all zero elements */
const kZeroVector = {
  2: toF32Vector([0, 0]),
  3: toF32Vector([0, 0, 0]),
  4: toF32Vector([0, 0, 0, 0]),
};

/** F32Vector with all F32Interval.any() elements */
const kAnyVector = {
  2: toF32Vector([F32Interval.any(), F32Interval.any()]),
  3: toF32Vector([F32Interval.any(), F32Interval.any(), F32Interval.any()]),
  4: toF32Vector([F32Interval.any(), F32Interval.any(), F32Interval.any(), F32Interval.any()]),
};

/**
 * @returns a F32Vector where each element is the span for corresponding
 *          elements at the same index in the input vectors
 */
function spanF32Vector(...vectors: F32Vector[]): F32Vector {
  const vector_length = vectors[0].length;
  assert(
    vectors.every(e => e.length === vector_length),
    `Vector span is not defined for vectors of differing lengths`
  );

  // The outer map is doing the walk across a single F32Vector to get the indices to use.
  // The inner map is doing the walk across the of the vector array, collecting the value of each vector at the
  // index, then spanning them down to a single F32Interval.
  // The toF32Vector coerces things at the end to be a F32Vector, because the outer .map() will actually return a
  // F32Interval[]
  return toF32Vector(
    vectors[0].map((_, idx) => {
      return F32Interval.span(...vectors.map(v => v[idx]));
    })
  );
}

/**
 * @retuns the vector result of multiplying the given vector by the given scalar
 */
function multiplyVectorByScalar(v: number[], c: number | F32Interval): F32Vector {
  return toF32Vector(v.map(x => multiplicationInterval(x, c)));
}

/**
 * @returns the input plus zero if any of the entries are f32 subnormal,
 * otherwise returns the input.
 */
function addFlushedIfNeededF32(values: number[]): number[] {
  return values.some(v => v !== 0 && isSubnormalNumberF32(v)) ? values.concat(0) : values;
}

/**
 * @returns the input plus zero if any of the entries are f16 subnormal,
 * otherwise returns the input
 */
function addFlushedIfNeededF16(values: number[]): number[] {
  return values.some(v => v !== 0 && isSubnormalNumberF16(v)) ? values.concat(0) : values;
}

/**
 * A function that converts a point to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface PointToInterval {
  (x: number): F32Interval;
}

/** Operation used to implement a PointToInterval */
export interface PointToIntervalOp {
  /** @returns acceptance interval for a function at point x */
  impl: PointToInterval;

  /**
   * Calculates where in the domain defined by x the min/max extrema of impl
   * occur and returns a span of those points to be used as the domain instead.
   *
   * Used by runPointToIntervalOp before invoking impl.
   * If not defined, the bounds of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for operations that meet all of the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   *      i.e. fooInterval takes in x: number | F32Interval, not x: number
   */
  extrema?: (x: F32Interval) => F32Interval;
}

/**
 * Restrict the inputs to an PointToInterval operation
 *
 * Only used for operations that have tighter domain requirements than 'must be
 * f32 finite'.
 *
 * @param domain interval to restrict inputs to
 * @param impl operation implementation to run if input is within the required domain
 * @returns a PointToInterval that calls impl if domain contains the input,
 *          otherwise it returns the any() interval */
function limitPointToIntervalDomain(domain: F32Interval, impl: PointToInterval): PointToInterval {
  return (n: number): F32Interval => {
    return domain.contains(n) ? impl(n) : F32Interval.any();
  };
}

/**
 * A function that converts a pair of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface BinaryToInterval {
  (x: number, y: number): F32Interval;
}

/** Operation used to implement a BinaryToInterval */
interface BinaryToIntervalOp {
  /** @returns acceptance interval for a function at point (x, y) */
  impl: BinaryToInterval;
  /**
   * Calculates where in domain defined by x & y the min/max extrema of impl
   * occur and returns spans of those points to be used as the domain instead.
   *
   * Used by runBinaryToIntervalOp before invoking impl.
   * If not defined, the bounds of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for functions that meet all of the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   */
  extrema?: (x: F32Interval, y: F32Interval) => [F32Interval, F32Interval];
}

/** Domain for a BinaryToInterval implementation */
interface BinaryToIntervalDomain {
  // Arrays to support discrete valid domain intervals
  x: F32Interval[];
  y: F32Interval[];
}

/**
 * Restrict the inputs to a BinaryToInterval
 *
 * Only used for operations that have tighter domain requirements than 'must be
 * f32 finite'.
 *
 * @param domain set of intervals to restrict inputs to
 * @param impl operation implementation to run if input is within the required domain
 * @returns a BinaryToInterval that calls impl if domain contains the input,
 *          otherwise it returns the any() interval */
function limitBinaryToIntervalDomain(
  domain: BinaryToIntervalDomain,
  impl: BinaryToInterval
): BinaryToInterval {
  return (x: number, y: number): F32Interval => {
    if (!domain.x.some(d => d.contains(x)) || !domain.y.some(d => d.contains(y))) {
      return F32Interval.any();
    }

    return impl(x, y);
  };
}

/**
 * A function that converts a triplet of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface TernaryToInterval {
  (x: number, y: number, z: number): F32Interval;
}

/** Operation used to implement a TernaryToInterval */
interface TernaryToIntervalOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns acceptance interval for a function at point (x, y, z) */
  impl: TernaryToInterval;
}

// Currently PointToVector is not integrated with the rest of the floating point
// framework, because the only builtins that use it are actually
// u32 -> [f32, f32, f32, f32] functions, so the whole rounding and interval
// process doesn't get applied to the inputs.
// They do use the framework internally by invoking divisionInterval on segments
// of the input.
/**
 * A function that converts a point to a vector of acceptance intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface PointToVector {
  (n: number): F32Vector;
}

/**
 * A function that converts a vector to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorToInterval {
  (x: number[]): F32Interval;
}

/** Operation used to implement a VectorToInterval */
interface VectorToIntervalOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns acceptance interval for a function on vector x */
  impl: VectorToInterval;
}

/**
 * A function that converts a pair of vectors to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorPairToInterval {
  (x: number[], y: number[]): F32Interval;
}

/** Operation used to implement a VectorPairToInterval */
interface VectorPairToIntervalOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns acceptance interval for a function on vectors (x, y) */
  impl: VectorPairToInterval;
}

/**
 * A function that converts a vector to a vector of acceptance intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorToVector {
  (x: number[]): F32Vector;
}

/** Operation used to implement a VectorToVector */
interface VectorToVectorOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns a vector of acceptance intervals for a function on vector x */
  impl: VectorToVector;
}

/**
 * A function that converts a pair of vectors to a vector of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorPairToVector {
  (x: number[], y: number[]): F32Vector;
}

/** Operation used to implement a VectorPairToVector */
interface VectorPairToVectorOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns a vector of acceptance intervals for a function on vectors (x, y) */
  impl: VectorPairToVector;
}

/** Converts a point to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * op.extrema is invoked before this point in the call stack.
 * op.domain is tested before this point in the call stack.
 *
 * @param n value to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushPointToInterval(n: number, op: PointToIntervalOp) {
  assert(!Number.isNaN(n), `flush not defined for NaN`);
  const values = correctlyRoundedF32(n);
  const inputs = addFlushedIfNeededF32(values);
  const results = new Set<F32Interval>(inputs.map(op.impl));
  return F32Interval.span(...results);
}

/** Converts a pair to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * All unique combinations of x & y are run.
 * op.extrema is invoked before this point in the call stack.
 * op.domain is tested before this point in the call stack.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param y second param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushBinaryToInterval(x: number, y: number, op: BinaryToIntervalOp): F32Interval {
  assert(!Number.isNaN(x), `flush not defined for NaN`);
  assert(!Number.isNaN(y), `flush not defined for NaN`);
  const x_values = correctlyRoundedF32(x);
  const y_values = correctlyRoundedF32(y);
  const x_inputs = addFlushedIfNeededF32(x_values);
  const y_inputs = addFlushedIfNeededF32(y_values);
  const intervals = new Set<F32Interval>();
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      intervals.add(op.impl(inner_x, inner_y));
    });
  });
  return F32Interval.span(...intervals);
}

/** Converts a triplet to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * All unique combinations of x, y & z are run.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param y second param to flush & round then invoke op.impl on
 * @param z third param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushTernaryToInterval(
  x: number,
  y: number,
  z: number,
  op: TernaryToIntervalOp
): F32Interval {
  assert(!Number.isNaN(x), `flush not defined for NaN`);
  assert(!Number.isNaN(y), `flush not defined for NaN`);
  assert(!Number.isNaN(z), `flush not defined for NaN`);
  const x_values = correctlyRoundedF32(x);
  const y_values = correctlyRoundedF32(y);
  const z_values = correctlyRoundedF32(z);
  const x_inputs = addFlushedIfNeededF32(x_values);
  const y_inputs = addFlushedIfNeededF32(y_values);
  const z_inputs = addFlushedIfNeededF32(z_values);
  const intervals = new Set<F32Interval>();
  // prettier-ignore
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      z_inputs.forEach(inner_z => {
        intervals.add(op.impl(inner_x, inner_y, inner_z));
      });
    });
  });

  return F32Interval.span(...intervals);
}

/** Converts a vector to an acceptance interval using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 *
 * @param x param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushVectorToInterval(x: number[], op: VectorToIntervalOp): F32Interval {
  assert(
    x.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );

  const x_rounded: number[][] = x.map(correctlyRoundedF32);
  const x_flushed: number[][] = x_rounded.map(addFlushedIfNeededF32);
  const x_inputs = cartesianProduct<number>(...x_flushed);

  const intervals = new Set<F32Interval>();
  x_inputs.forEach(inner_x => {
    intervals.add(op.impl(inner_x));
  });
  return F32Interval.span(...intervals);
}

/** Converts a pair of vectors to an acceptance interval using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * All unique combinations of x & y are run.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param y second param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushVectorPairToInterval(
  x: number[],
  y: number[],
  op: VectorPairToIntervalOp
): F32Interval {
  assert(
    x.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );
  assert(
    y.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );

  const x_rounded: number[][] = x.map(correctlyRoundedF32);
  const y_rounded: number[][] = y.map(correctlyRoundedF32);
  const x_flushed: number[][] = x_rounded.map(addFlushedIfNeededF32);
  const y_flushed: number[][] = y_rounded.map(addFlushedIfNeededF32);
  const x_inputs = cartesianProduct<number>(...x_flushed);
  const y_inputs = cartesianProduct<number>(...y_flushed);

  const intervals = new Set<F32Interval>();
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      intervals.add(op.impl(inner_x, inner_y));
    });
  });
  return F32Interval.span(...intervals);
}

/** Converts a vector to a vector of acceptance intervals using a specific
 * function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 *
 * @param x param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a vector of spans for each outputs of op.impl
 */
function roundAndFlushVectorToVector(x: number[], op: VectorToVectorOp): F32Vector {
  assert(
    x.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );

  const x_rounded: number[][] = x.map(correctlyRoundedF32);
  const x_flushed: number[][] = x_rounded.map(addFlushedIfNeededF32);
  const x_inputs = cartesianProduct<number>(...x_flushed);

  const interval_vectors = new Set<F32Vector>();
  x_inputs.forEach(inner_x => {
    interval_vectors.add(op.impl(inner_x));
  });

  return spanF32Vector(...interval_vectors);
}

/**
 * Converts a pair of vectors to a vector of acceptance intervals using a
 * specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param x second param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a vector of spans for each output of op.impl
 */
function roundAndFlushVectorPairToVector(
  x: number[],
  y: number[],
  op: VectorPairToVectorOp
): F32Vector {
  assert(
    x.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );
  assert(
    y.every(e => !Number.isNaN(e)),
    `flush not defined for NaN`
  );

  const x_rounded: number[][] = x.map(correctlyRoundedF32);
  const y_rounded: number[][] = y.map(correctlyRoundedF32);
  const x_flushed: number[][] = x_rounded.map(addFlushedIfNeededF32);
  const y_flushed: number[][] = y_rounded.map(addFlushedIfNeededF32);
  const x_inputs = cartesianProduct<number>(...x_flushed);
  const y_inputs = cartesianProduct<number>(...y_flushed);

  const interval_vectors = new Set<F32Vector>();
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      interval_vectors.add(op.impl(inner_x, inner_y));
    });
  });

  return spanF32Vector(...interval_vectors);
}

/** Calculate the acceptance interval for a unary function over an interval
 *
 * If the interval is actually a point, this just decays to
 * roundAndFlushPointToInterval.
 *
 * The provided domain interval may be adjusted if the operation defines an
 * extrema function.
 *
 * @param x input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function runPointToIntervalOp(x: F32Interval, op: PointToIntervalOp): F32Interval {
  if (!x.isFinite()) {
    return F32Interval.any();
  }

  if (op.extrema !== undefined) {
    x = op.extrema(x);
  }

  const result = F32Interval.span(...x.bounds().map(b => roundAndFlushPointToInterval(b, op)));
  return result.isFinite() ? result : F32Interval.any();
}

/** Calculate the acceptance interval for a binary function over an interval
 *
 * The provided domain intervals may be adjusted if the operation defines an
 * extrema function.
 *
 * @param x first input domain interval
 * @param y second input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function runBinaryToIntervalOp(
  x: F32Interval,
  y: F32Interval,
  op: BinaryToIntervalOp
): F32Interval {
  if (!x.isFinite() || !y.isFinite()) {
    return F32Interval.any();
  }

  if (op.extrema !== undefined) {
    [x, y] = op.extrema(x, y);
  }

  const outputs = new Set<F32Interval>();
  x.bounds().forEach(inner_x => {
    y.bounds().forEach(inner_y => {
      outputs.add(roundAndFlushBinaryToInterval(inner_x, inner_y, op));
    });
  });

  const result = F32Interval.span(...outputs);
  return result.isFinite() ? result : F32Interval.any();
}

/** Calculate the acceptance interval for a ternary function over an interval
 *
 * @param x first input domain interval
 * @param y second input domain interval
 * @param z third input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function runTernaryToIntervalOp(
  x: F32Interval,
  y: F32Interval,
  z: F32Interval,
  op: TernaryToIntervalOp
): F32Interval {
  if (!x.isFinite() || !y.isFinite() || !z.isFinite()) {
    return F32Interval.any();
  }

  const outputs = new Set<F32Interval>();
  x.bounds().forEach(inner_x => {
    y.bounds().forEach(inner_y => {
      z.bounds().forEach(inner_z => {
        outputs.add(roundAndFlushTernaryToInterval(inner_x, inner_y, inner_z, op));
      });
    });
  });

  const result = F32Interval.span(...outputs);
  return result.isFinite() ? result : F32Interval.any();
}

/** Calculate the acceptance interval for a vector function over given intervals
 *
 * @param x input domain intervals vector
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function runVectorToIntervalOp(x: F32Vector, op: VectorToIntervalOp): F32Interval {
  if (x.some(e => !e.isFinite())) {
    return F32Interval.any();
  }

  const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));

  const outputs = new Set<F32Interval>();
  x_values.forEach(inner_x => {
    outputs.add(roundAndFlushVectorToInterval(inner_x, op));
  });

  const result = F32Interval.span(...outputs);
  return result.isFinite() ? result : F32Interval.any();
}

/** Calculate the acceptance interval for a vector pair function over given intervals
 *
 * @param x first input domain intervals vector
 * @param y second input domain intervals vector
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function runVectorPairToIntervalOp(
  x: F32Vector,
  y: F32Vector,
  op: VectorPairToIntervalOp
): F32Interval {
  if (x.some(e => !e.isFinite()) || y.some(e => !e.isFinite())) {
    return F32Interval.any();
  }

  const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));
  const y_values = cartesianProduct<number>(...y.map(e => e.bounds()));

  const outputs = new Set<F32Interval>();
  x_values.forEach(inner_x => {
    y_values.forEach(inner_y => {
      outputs.add(roundAndFlushVectorPairToInterval(inner_x, inner_y, op));
    });
  });

  const result = F32Interval.span(...outputs);
  return result.isFinite() ? result : F32Interval.any();
}

/** Calculate the vector of acceptance intervals for a pair of vector function over
 * given intervals
 *
 * @param x input domain intervals vector
 * @param x input domain intervals vector
 * @param op operation defining the function being run
 * @returns a vector of spans over all of the outputs of op.impl
 */
function runVectorToVectorOp(x: F32Vector, op: VectorToVectorOp): F32Vector {
  if (x.some(e => !e.isFinite())) {
    return kAnyVector[x.length];
  }

  const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));

  const outputs = new Set<F32Vector>();
  x_values.forEach(inner_x => {
    outputs.add(roundAndFlushVectorToVector(inner_x, op));
  });

  const result = spanF32Vector(...outputs);
  return result.every(e => e.isFinite()) ? result : toF32Vector(x.map(_ => F32Interval.any()));
}

/**
 * Calculate the vector of acceptance intervals by running a scalar operation
 * component-wise over a vector.
 *
 * This is used for situations where a component-wise operation, like vector
 * negation, is needed as part of a inherited accuracy, but the top-level
 * operation test don't require an explicit vector definition of the function,
 * due to the generated vectorize tests being sufficient.
 *
 * @param x input domain intervals vector
 * @param op scalar operation to be run component-wise
 * @returns a vector of intervals with the outputs of op.impl
 */
function runPointToIntervalOpComponentWise(x: F32Vector, op: PointToIntervalOp): F32Vector {
  return toF32Vector(
    x.map(i => {
      return runPointToIntervalOp(i, op);
    })
  );
}

/** Calculate the vector of acceptance intervals for a vector function over
 * given intervals
 *
 * @param x first input domain intervals vector
 * @param y second input domain intervals vector
 * @param op operation defining the function being run
 * @returns a vector of spans over all of the outputs of op.impl
 */
function runVectorPairToVectorOp(x: F32Vector, y: F32Vector, op: VectorPairToVectorOp): F32Vector {
  if (x.some(e => !e.isFinite()) || y.some(e => !e.isFinite())) {
    return kAnyVector[x.length];
  }

  const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));
  const y_values = cartesianProduct<number>(...y.map(e => e.bounds()));

  const outputs = new Set<F32Vector>();
  x_values.forEach(inner_x => {
    y_values.forEach(inner_y => {
      outputs.add(roundAndFlushVectorPairToVector(inner_x, inner_y, op));
    });
  });

  const result = spanF32Vector(...outputs);
  return result.every(e => e.isFinite()) ? result : toF32Vector(x.map(_ => F32Interval.any()));
}

/**
 * Calculate the vector of acceptance intervals by running a scalar operation
 * component-wise over a pair vectors.
 *
 * This is used for situations where a component-wise operation, like vector
 * subtraction, is needed as part of a inherited accuracy, but the top-level
 * operation test don't require an explicit vector definition of the function,
 * due to the generated vectorize tests being sufficient.
 *
 * @param x first input domain intervals vector
 * @param y second input domain intervals vector
 * @param op scalar operation to be run component-wise
 * @returns a vector of intervals with the outputs of op.impl
 */
function runBinaryToIntervalOpComponentWise(
  x: F32Vector,
  y: F32Vector,
  op: BinaryToIntervalOp
): F32Vector {
  assert(
    x.length === y.length,
    `runBinaryToIntervalOpComponentWise requires vectors of the same length`
  );
  return toF32Vector(
    x.map((i, idx) => {
      return runBinaryToIntervalOp(i, y[idx], op);
    })
  );
}

/** Defines a PointToIntervalOp for an interval of the correctly rounded values around the point */
const CorrectlyRoundedIntervalOp: PointToIntervalOp = {
  impl: (n: number) => {
    assert(!Number.isNaN(n), `absolute not defined for NaN`);
    return toF32Interval(n);
  },
};

/** @returns an interval of the correctly rounded values around the point */
export function correctlyRoundedInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), CorrectlyRoundedIntervalOp);
}

/** @returns a PointToIntervalOp for [n - error_range, n + error_range] */
function AbsoluteErrorIntervalOp(error_range: number): PointToIntervalOp {
  const op: PointToIntervalOp = {
    impl: (_: number) => {
      return F32Interval.any();
    },
  };

  if (isFiniteF32(error_range)) {
    op.impl = (n: number) => {
      assert(!Number.isNaN(n), `absolute error not defined for NaN`);
      return toF32Interval([n - error_range, n + error_range]);
    };
  }

  return op;
}

/** @returns an interval of the absolute error around the point */
export function absoluteErrorInterval(n: number, error_range: number): F32Interval {
  error_range = Math.abs(error_range);
  return runPointToIntervalOp(toF32Interval(n), AbsoluteErrorIntervalOp(error_range));
}

/** @returns a PointToIntervalOp for [n - numULP * ULP(n), n + numULP * ULP(n)] */
function ULPIntervalOp(numULP: number): PointToIntervalOp {
  const op: PointToIntervalOp = {
    impl: (_: number) => {
      return F32Interval.any();
    },
  };

  if (isFiniteF32(numULP)) {
    op.impl = (n: number) => {
      assert(!Number.isNaN(n), `ULP error not defined for NaN`);

      const ulp = oneULP(n);
      const begin = n - numULP * ulp;
      const end = n + numULP * ulp;

      return toF32Interval([
        Math.min(begin, flushSubnormalNumberF32(begin)),
        Math.max(end, flushSubnormalNumberF32(end)),
      ]);
    };
  }

  return op;
}

/** @returns an interval of N * ULP around the point */
export function ulpInterval(n: number, numULP: number): F32Interval {
  numULP = Math.abs(numULP);
  return runPointToIntervalOp(toF32Interval(n), ULPIntervalOp(numULP));
}

const AbsIntervalOp: PointToIntervalOp = {
  impl: (n: number) => {
    return correctlyRoundedInterval(Math.abs(n));
  },
};

/** Calculate an acceptance interval for abs(n) */
export function absInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AbsIntervalOp);
}

const AcosIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(toF32Interval([-1.0, 1.0]), (n: number) => {
    // acos(n) = atan2(sqrt(1.0 - n * n), n)
    const y = sqrtInterval(subtractionInterval(1, multiplicationInterval(n, n)));
    return atan2Interval(y, n);
  }),
};

/** Calculate an acceptance interval for acos(n) */
export function acosInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AcosIntervalOp);
}

/** All acceptance interval functions for acosh(x) */
export const acoshIntervals: PointToInterval[] = [acoshAlternativeInterval, acoshPrimaryInterval];

const AcoshAlternativeIntervalOp: PointToIntervalOp = {
  impl: (x: number): F32Interval => {
    // acosh(x) = log(x + sqrt((x + 1.0f) * (x - 1.0)))
    const inner_value = multiplicationInterval(
      additionInterval(x, 1.0),
      subtractionInterval(x, 1.0)
    );
    const sqrt_value = sqrtInterval(inner_value);
    return logInterval(additionInterval(x, sqrt_value));
  },
};

/** Calculate an acceptance interval of acosh(x) using log(x + sqrt((x + 1.0f) * (x - 1.0))) */
export function acoshAlternativeInterval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), AcoshAlternativeIntervalOp);
}

const AcoshPrimaryIntervalOp: PointToIntervalOp = {
  impl: (x: number): F32Interval => {
    // acosh(x) = log(x + sqrt(x * x - 1.0))
    const inner_value = subtractionInterval(multiplicationInterval(x, x), 1.0);
    const sqrt_value = sqrtInterval(inner_value);
    return logInterval(additionInterval(x, sqrt_value));
  },
};

/** Calculate an acceptance interval of acosh(x) using log(x + sqrt(x * x - 1.0)) */
export function acoshPrimaryInterval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), AcoshPrimaryIntervalOp);
}

const AdditionIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return correctlyRoundedInterval(x + y);
  },
};

/** Calculate an acceptance interval of x + y */
export function additionInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), AdditionIntervalOp);
}

const AsinIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(toF32Interval([-1.0, 1.0]), (n: number) => {
    // asin(n) = atan2(n, sqrt(1.0 - n * n))
    const x = sqrtInterval(subtractionInterval(1, multiplicationInterval(n, n)));
    return atan2Interval(n, x);
  }),
};

/** Calculate an acceptance interval for asin(n) */
export function asinInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AsinIntervalOp);
}

const AsinhIntervalOp: PointToIntervalOp = {
  impl: (x: number): F32Interval => {
    // asinh(x) = log(x + sqrt(x * x + 1.0))
    const inner_value = additionInterval(multiplicationInterval(x, x), 1.0);
    const sqrt_value = sqrtInterval(inner_value);
    return logInterval(additionInterval(x, sqrt_value));
  },
};

/** Calculate an acceptance interval of asinh(x) */
export function asinhInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AsinhIntervalOp);
}

const AtanIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return ulpInterval(Math.atan(n), 4096);
  },
};

/** Calculate an acceptance interval of atan(x) */
export function atanInterval(n: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AtanIntervalOp);
}

const Atan2IntervalOp: BinaryToIntervalOp = {
  impl: limitBinaryToIntervalDomain(
    {
      // For atan2, there params are labelled (y, x), not (x, y), so domain.x is first parameter (y), and domain.y is
      // the second parameter (x)
      x: [
        toF32Interval([kValue.f32.negative.min, kValue.f32.negative.max]),
        toF32Interval([kValue.f32.positive.min, kValue.f32.positive.max]),
      ], // first param must be finite and normal
      y: [toF32Interval([-(2 ** 126), -(2 ** -126)]), toF32Interval([2 ** -126, 2 ** 126])], // inherited from division
    },
    (y: number, x: number): F32Interval => {
      const atan_yx = Math.atan(y / x);
      // x > 0, atan(y/x)
      if (x > 0) {
        return ulpInterval(atan_yx, 4096);
      }

      // x < 0, y > 0, atan(y/x) + π
      if (y > 0) {
        return ulpInterval(atan_yx + kValue.f32.positive.pi.whole, 4096);
      }

      // x < 0, y < 0, atan(y/x) - π
      return ulpInterval(atan_yx - kValue.f32.positive.pi.whole, 4096);
    }
  ),
  extrema: (y: F32Interval, x: F32Interval): [F32Interval, F32Interval] => {
    // There is discontinuity + undefined behaviour at y/x = 0 that will dominate the accuracy
    if (y.contains(0)) {
      if (x.contains(0)) {
        return [toF32Interval(0), toF32Interval(0)];
      }
      return [toF32Interval(0), x];
    }
    return [y, x];
  },
};

/** Calculate an acceptance interval of atan2(y, x) */
export function atan2Interval(y: number | F32Interval, x: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(y), toF32Interval(x), Atan2IntervalOp);
}

const AtanhIntervalOp: PointToIntervalOp = {
  impl: (n: number) => {
    // atanh(x) = log((1.0 + x) / (1.0 - x)) * 0.5
    const numerator = additionInterval(1.0, n);
    const denominator = subtractionInterval(1.0, n);
    const log_interval = logInterval(divisionInterval(numerator, denominator));
    return multiplicationInterval(log_interval, 0.5);
  },
};

/** Calculate an acceptance interval of atanh(x) */
export function atanhInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), AtanhIntervalOp);
}

const CeilIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return correctlyRoundedInterval(Math.ceil(n));
  },
};

/** Calculate an acceptance interval of ceil(x) */
export function ceilInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), CeilIntervalOp);
}

const ClampMedianIntervalOp: TernaryToIntervalOp = {
  impl: (x: number, y: number, z: number): F32Interval => {
    return correctlyRoundedInterval(
      // Default sort is string sort, so have to implement numeric comparison.
      // Cannot use the b-a one liner, because that assumes no infinities.
      [x, y, z].sort((a, b) => {
        if (a < b) {
          return -1;
        }
        if (a > b) {
          return 1;
        }
        return 0;
      })[1]
    );
  },
};

/** All acceptance interval functions for clamp(x, y, z) */
export const clampIntervals: TernaryToInterval[] = [clampMinMaxInterval, clampMedianInterval];

/** Calculate an acceptance interval of clamp(x, y, z) via median(x, y, z) */
export function clampMedianInterval(
  x: number | F32Interval,
  y: number | F32Interval,
  z: number | F32Interval
): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(x),
    toF32Interval(y),
    toF32Interval(z),
    ClampMedianIntervalOp
  );
}

const ClampMinMaxIntervalOp: TernaryToIntervalOp = {
  impl: (x: number, low: number, high: number): F32Interval => {
    return correctlyRoundedInterval(Math.min(Math.max(x, low), high));
  },
};

/** Calculate an acceptance interval of clamp(x, high, low) via min(max(x, low), high) */
export function clampMinMaxInterval(
  x: number | F32Interval,
  low: number | F32Interval,
  high: number | F32Interval
): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(x),
    toF32Interval(low),
    toF32Interval(high),
    ClampMinMaxIntervalOp
  );
}

const CosIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(
    kNegPiToPiInterval,
    (n: number): F32Interval => {
      return absoluteErrorInterval(Math.cos(n), 2 ** -11);
    }
  ),
};

/** Calculate an acceptance interval of cos(x) */
export function cosInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), CosIntervalOp);
}

const CoshIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    // cosh(x) = (exp(x) + exp(-x)) * 0.5
    const minus_n = negationInterval(n);
    return multiplicationInterval(additionInterval(expInterval(n), expInterval(minus_n)), 0.5);
  },
};

/** Calculate an acceptance interval of cosh(x) */
export function coshInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), CoshIntervalOp);
}

const CrossIntervalOp: VectorPairToVectorOp = {
  impl: (x: number[], y: number[]): F32Vector => {
    assert(x.length === 3, `CrossIntervalOp received x with ${x.length} instead of 3`);
    assert(y.length === 3, `CrossIntervalOp received y with ${y.length} instead of 3`);

    // cross(x, y) = r, where
    //   r[0] = x[1] * y[2] - x[2] * y[1]
    //   r[1] = x[2] * y[0] - x[0] * y[2]
    //   r[2] = x[0] * y[1] - x[1] * y[0]

    const r0 = subtractionInterval(
      multiplicationInterval(x[1], y[2]),
      multiplicationInterval(x[2], y[1])
    );
    const r1 = subtractionInterval(
      multiplicationInterval(x[2], y[0]),
      multiplicationInterval(x[0], y[2])
    );
    const r2 = subtractionInterval(
      multiplicationInterval(x[0], y[1]),
      multiplicationInterval(x[1], y[0])
    );
    return [r0, r1, r2];
  },
};

export function crossInterval(x: number[], y: number[]): F32Vector {
  assert(x.length === 3, `Cross is only defined for vec3`);
  assert(y.length === 3, `Cross is only defined for vec3`);
  return runVectorPairToVectorOp(toF32Vector(x), toF32Vector(y), CrossIntervalOp);
}

const DegreesIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return multiplicationInterval(n, 57.295779513082322865);
  },
};

/** Calculate an acceptance interval of degrees(x) */
export function degreesInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), DegreesIntervalOp);
}

const DistanceIntervalScalarOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return lengthInterval(subtractionInterval(x, y));
  },
};

const DistanceIntervalVectorOp: VectorPairToIntervalOp = {
  impl: (x: number[], y: number[]): F32Interval => {
    return lengthInterval(
      runBinaryToIntervalOpComponentWise(toF32Vector(x), toF32Vector(y), SubtractionIntervalOp)
    );
  },
};

/** Calculate an acceptance interval of distance(x, y) */
export function distanceInterval(x: number | number[], y: number | number[]): F32Interval {
  if (x instanceof Array && y instanceof Array) {
    assert(
      x.length === y.length,
      `distanceInterval requires both params to have the same number of elements`
    );
    return runVectorPairToIntervalOp(toF32Vector(x), toF32Vector(y), DistanceIntervalVectorOp);
  } else if (!(x instanceof Array) && !(y instanceof Array)) {
    return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), DistanceIntervalScalarOp);
  }
  unreachable(
    `distanceInterval requires both params to both the same type, either scalars or vectors`
  );
}

const DivisionIntervalOp: BinaryToIntervalOp = {
  impl: limitBinaryToIntervalDomain(
    {
      x: [toF32Interval([kValue.f32.negative.min, kValue.f32.positive.max])],
      y: [toF32Interval([-(2 ** 126), -(2 ** -126)]), toF32Interval([2 ** -126, 2 ** 126])],
    },
    (x: number, y: number): F32Interval => {
      if (y === 0) {
        return F32Interval.any();
      }
      return ulpInterval(x / y, 2.5);
    }
  ),
  extrema: (x: F32Interval, y: F32Interval): [F32Interval, F32Interval] => {
    // division has a discontinuity at y = 0.
    if (y.contains(0)) {
      y = toF32Interval(0);
    }
    return [x, y];
  },
};

/** Calculate an acceptance interval of x / y */
export function divisionInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), DivisionIntervalOp);
}

const DotIntervalOp: VectorPairToIntervalOp = {
  impl: (x: number[], y: number[]): F32Interval => {
    // dot(x, y) = sum of x[i] * y[i]
    const multiplications = runBinaryToIntervalOpComponentWise(
      toF32Vector(x),
      toF32Vector(y),
      MultiplicationIntervalOp
    );

    // vec2 doesn't require permutations, since a + b = b + a for floats
    if (multiplications.length === 2) {
      return additionInterval(multiplications[0], multiplications[1]);
    }

    // The spec does not state the ordering of summation, so all of the permutations are calculated and their results
    // spanned, since addition of more then two floats is not transitive, i.e. a + b + c is not guaranteed to equal
    // b + a + c
    const permutations: F32Interval[][] = calculatePermutations(multiplications);
    return F32Interval.span(
      ...permutations.map(p => p.reduce((prev, cur) => additionInterval(prev, cur)))
    );
  },
};

export function dotInterval(x: number[], y: number[]): F32Interval {
  assert(x.length === y.length, `dot not defined for vectors with different lengths`);
  return runVectorPairToIntervalOp(toF32Vector(x), toF32Vector(y), DotIntervalOp);
}

const ExpIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return ulpInterval(Math.exp(n), 3 + 2 * Math.abs(n));
  },
};

/** Calculate an acceptance interval for exp(x) */
export function expInterval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), ExpIntervalOp);
}

const Exp2IntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return ulpInterval(Math.pow(2, n), 3 + 2 * Math.abs(n));
  },
};

/** Calculate an acceptance interval for exp2(x) */
export function exp2Interval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), Exp2IntervalOp);
}

/**
 * Calculate the acceptance intervals for faceForward(x, y, z)
 *
 * faceForward(x, y, z) = select(-x, x, dot(z, y) < 0.0)
 *
 * This builtin selects from two discrete results (delta rounding/flushing), so
 * the majority of the framework code is not appropriate, since the framework
 * attempts to span results.
 *
 * Thus a bespoke implementation is used instead of
 * defining a Op and running that through the framework.
 */
export function faceForwardIntervals(
  x: number[],
  y: number[],
  z: number[]
): (F32Vector | undefined)[] {
  const x_vec = toF32Vector(x);
  // Running vector through runPointToIntervalOpComponentWise to make sure that flushing/rounding is handled, since
  // toF32Vector does not perform those operations.
  const positive_x = runPointToIntervalOpComponentWise(x_vec, { impl: toF32Interval });
  const negative_x = runPointToIntervalOpComponentWise(x_vec, NegationIntervalOp);

  const dot_interval = dotInterval(z, y);

  const results: (F32Vector | undefined)[] = [];

  if (!dot_interval.isFinite()) {
    // dot calculation went out of bounds
    // Inserting undefine in the result, so that the test running framework is aware
    // of this potential OOB.
    // For const-eval tests, it means that the test case should be skipped,
    // since the shader will fail to compile.
    // For non-const-eval the undefined should be stripped out of the possible
    // results.

    results.push(undefined);
  }

  // Because the result of dot can be an interval, it might span across 0, thus
  // it is possible that both -x and x are valid responses.
  if (dot_interval.begin < 0 || dot_interval.end < 0) {
    results.push(positive_x);
  }

  if (dot_interval.begin >= 0 || dot_interval.end >= 0) {
    results.push(negative_x);
  }

  assert(
    results.length > 0 || results.every(r => r === undefined),
    `faceForwardInterval selected neither positive x or negative x for the result, this shouldn't be possible`
  );
  return results;
}

const FloorIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return correctlyRoundedInterval(Math.floor(n));
  },
};

/** Calculate an acceptance interval of floor(x) */
export function floorInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), FloorIntervalOp);
}

const FmaIntervalOp: TernaryToIntervalOp = {
  impl: (x: number, y: number, z: number): F32Interval => {
    return additionInterval(multiplicationInterval(x, y), z);
  },
};

/** Calculate an acceptance interval for fma(x, y, z) */
export function fmaInterval(x: number, y: number, z: number): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(x),
    toF32Interval(y),
    toF32Interval(z),
    FmaIntervalOp
  );
}

const FractIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    // fract(x) = x - floor(x) is defined in the spec.
    // For people coming from a non-graphics background this will cause some unintuitive results. For example,
    // fract(-1.1) is not 0.1 or -0.1, but instead 0.9.
    // This is how other shading languages operate and allows for a desirable wrap around in graphics programming.
    const result = subtractionInterval(n, floorInterval(n));
    if (result.contains(1)) {
      // Very small negative numbers can lead to catastrophic cancellation, thus calculating a fract of 1.0, which is
      // technically not a fractional part, so some implementations clamp the result to next nearest number.
      return F32Interval.span(result, toF32Interval(kValue.f32.positive.less_than_one));
    }
    return result;
  },
};

/** Calculate an acceptance interval of fract(x) */
export function fractInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), FractIntervalOp);
}

const InverseSqrtIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(
    kGreaterThanZeroInterval,
    (n: number): F32Interval => {
      return ulpInterval(1 / Math.sqrt(n), 2);
    }
  ),
};

/** Calculate an acceptance interval of inverseSqrt(x) */
export function inverseSqrtInterval(n: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), InverseSqrtIntervalOp);
}

const LdexpIntervalOp: BinaryToIntervalOp = {
  impl: limitBinaryToIntervalDomain(
    // Implementing SPIR-V's more restrictive domain until
    // https://github.com/gpuweb/gpuweb/issues/3134 is resolved
    {
      x: [toF32Interval([kValue.f32.negative.min, kValue.f32.positive.max])],
      y: [toF32Interval([-126, 128])],
    },
    (e1: number, e2: number): F32Interval => {
      // Though the spec says the result of ldexp(e1, e2) = e1 * 2 ^ e2, the
      // accuracy is listed as correctly rounded to the true value, so the
      // inheritance framework does not need to be invoked to determine bounds.
      // Instead the value at a higher precision is calculated and passed to
      // correctlyRoundedInterval.
      const result = e1 * 2 ** e2;
      if (Number.isNaN(result)) {
        // Overflowed TS's number type, so definitely out of bounds for f32
        return F32Interval.any();
      }
      return correctlyRoundedInterval(result);
    }
  ),
};

/** Calculate an acceptance interval of ldexp(e1, e2) */
export function ldexpInterval(e1: number, e2: number): F32Interval {
  return roundAndFlushBinaryToInterval(e1, e2, LdexpIntervalOp);
}

const LengthIntervalScalarOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return sqrtInterval(multiplicationInterval(n, n));
  },
};

const LengthIntervalVectorOp: VectorToIntervalOp = {
  impl: (n: number[]): F32Interval => {
    return sqrtInterval(dotInterval(n, n));
  },
};

/** Calculate an acceptance interval of length(x) */
export function lengthInterval(n: number | F32Interval | number[] | F32Vector): F32Interval {
  if (n instanceof Array) {
    return runVectorToIntervalOp(toF32Vector(n), LengthIntervalVectorOp);
  } else {
    return runPointToIntervalOp(toF32Interval(n), LengthIntervalScalarOp);
  }
}

const LogIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(
    kGreaterThanZeroInterval,
    (n: number): F32Interval => {
      if (n >= 0.5 && n <= 2.0) {
        return absoluteErrorInterval(Math.log(n), 2 ** -21);
      }
      return ulpInterval(Math.log(n), 3);
    }
  ),
};

/** Calculate an acceptance interval of log(x) */
export function logInterval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), LogIntervalOp);
}

const Log2IntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(
    kGreaterThanZeroInterval,
    (n: number): F32Interval => {
      if (n >= 0.5 && n <= 2.0) {
        return absoluteErrorInterval(Math.log2(n), 2 ** -21);
      }
      return ulpInterval(Math.log2(n), 3);
    }
  ),
};

/** Calculate an acceptance interval of log2(x) */
export function log2Interval(x: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(x), Log2IntervalOp);
}

const MaxIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return correctlyRoundedInterval(Math.max(x, y));
  },
};

/** Calculate an acceptance interval of max(x, y) */
export function maxInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), MaxIntervalOp);
}

const MinIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return correctlyRoundedInterval(Math.min(x, y));
  },
};

/** Calculate an acceptance interval of min(x, y) */
export function minInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), MinIntervalOp);
}

const MixImpreciseIntervalOp: TernaryToIntervalOp = {
  impl: (x: number, y: number, z: number): F32Interval => {
    // x + (y - x) * z =
    //  x + t, where t = (y - x) * z
    const t = multiplicationInterval(subtractionInterval(y, x), z);
    return additionInterval(x, t);
  },
};

/** All acceptance interval functions for mix(x, y, z) */
export const mixIntervals: TernaryToInterval[] = [mixImpreciseInterval, mixPreciseInterval];

/** Calculate an acceptance interval of mix(x, y, z) using x + (y - x) * z */
export function mixImpreciseInterval(x: number, y: number, z: number): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(x),
    toF32Interval(y),
    toF32Interval(z),
    MixImpreciseIntervalOp
  );
}

const MixPreciseIntervalOp: TernaryToIntervalOp = {
  impl: (x: number, y: number, z: number): F32Interval => {
    // x * (1.0 - z) + y * z =
    //   t + s, where t = x * (1.0 - z), s = y * z
    const t = multiplicationInterval(x, subtractionInterval(1.0, z));
    const s = multiplicationInterval(y, z);
    return additionInterval(t, s);
  },
};

/** Calculate an acceptance interval of mix(x, y, z) using x * (1.0 - z) + y * z */
export function mixPreciseInterval(x: number, y: number, z: number): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(x),
    toF32Interval(y),
    toF32Interval(z),
    MixPreciseIntervalOp
  );
}

/** Calculate an acceptance interval of modf(x) */
export function modfInterval(n: number): { fract: F32Interval; whole: F32Interval } {
  const fract = correctlyRoundedInterval(n % 1.0);
  const whole = correctlyRoundedInterval(n - (n % 1.0));
  return { fract, whole };
}

const MultiplicationInnerOp = {
  impl: (x: number, y: number): F32Interval => {
    return correctlyRoundedInterval(x * y);
  },
};

const MultiplicationIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return roundAndFlushBinaryToInterval(x, y, MultiplicationInnerOp);
  },
};

/** Calculate an acceptance interval of x * y */
export function multiplicationInterval(
  x: number | F32Interval,
  y: number | F32Interval
): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), MultiplicationIntervalOp);
}

const NegationIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return correctlyRoundedInterval(-n);
  },
};

/** Calculate an acceptance interval of -x */
export function negationInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), NegationIntervalOp);
}

const NormalizeIntervalOp: VectorToVectorOp = {
  impl: (n: number[]): F32Vector => {
    const length = lengthInterval(n);
    return toF32Vector(n.map(e => divisionInterval(e, length)));
  },
};

/** Calculate an acceptance interval of normalize(x) */
export function normalizeInterval(n: number[]): F32Vector {
  return runVectorToVectorOp(toF32Vector(n), NormalizeIntervalOp);
}

const PowIntervalOp: BinaryToIntervalOp = {
  // pow(x, y) has no explicit domain restrictions, but inherits the x <= 0
  // domain restriction from log2(x). Invoking log2Interval(x) in impl will
  // enforce this, so there is no need to wrap the impl call here.
  impl: (x: number, y: number): F32Interval => {
    return exp2Interval(multiplicationInterval(y, log2Interval(x)));
  },
};

/** Calculate an acceptance interval of pow(x, y) */
export function powInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), PowIntervalOp);
}

// Once a full implementation of F16Interval exists, the correctlyRounded for
// that can potentially be used instead of having a bespoke operation
// implementation.
const QuantizeToF16IntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    const rounded = correctlyRoundedF16(n);
    const flushed = addFlushedIfNeededF16(rounded);
    return F32Interval.span(...flushed.map(toF32Interval));
  },
};

/** Calculate an acceptance interval of quanitizeToF16(x) */
export function quantizeToF16Interval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), QuantizeToF16IntervalOp);
}

const RadiansIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return multiplicationInterval(n, 0.017453292519943295474);
  },
};

/** Calculate an acceptance interval of radians(x) */
export function radiansInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), RadiansIntervalOp);
}

const ReflectIntervalOp: VectorPairToVectorOp = {
  impl: (x: number[], y: number[]): F32Vector => {
    assert(
      x.length === y.length,
      `ReflectIntervalOp received x (${x}) and y (${y}) with different numbers of elements`
    );

    // reflect(x, y) = x - 2.0 * dot(x, y) * y
    //               = x - t * y, t = 2.0 * dot(x, y)
    // x = incident vector
    // y = normal of reflecting surface
    const t = multiplicationInterval(2.0, dotInterval(x, y));
    const rhs = multiplyVectorByScalar(y, t);
    return runBinaryToIntervalOpComponentWise(toF32Vector(x), rhs, SubtractionIntervalOp);
  },
};

/** Calculate an acceptance interval of reflect(x, y) */
export function reflectInterval(x: number[], y: number[]): F32Vector {
  assert(
    x.length === y.length,
    `reflect is only defined for vectors with the same number of elements`
  );
  return runVectorPairToVectorOp(toF32Vector(x), toF32Vector(y), ReflectIntervalOp);
}

/**
 * Calculate acceptance interval vectors of reflect(i, s, r)
 *
 * refract is a singular function in the sense that it is the only builtin that
 * takes in (F32Vector, F32Vector, F32) and returns F32Vector and is basically
 * defined in terms of other functions.
 *
 * Instead of implementing all of the framework code to integrate it with its
 * own operation type/etc, it instead has a bespoke implementation that is a
 * composition of other builtin functions that use the framework.
 */
export function refractInterval(i: number[], s: number[], r: number): F32Vector {
  assert(
    i.length === s.length,
    `refract is only defined for vectors with the same number of elements`
  );

  const r_squared = multiplicationInterval(r, r);
  const dot = dotInterval(s, i);
  const dot_squared = multiplicationInterval(dot, dot);
  const one_minus_dot_squared = subtractionInterval(1, dot_squared);
  const k = subtractionInterval(1.0, multiplicationInterval(r_squared, one_minus_dot_squared));

  if (!k.isFinite() || k.containsZeroOrSubnormals()) {
    // There is a discontinuity at k == 0, due to sqrt(k) being calculated, so exiting early
    return kAnyVector[toF32Vector(i).length];
  }

  if (k.end < 0.0) {
    // if k is negative, then the zero vector is the valid response
    return kZeroVector[toF32Vector(i).length];
  }

  const dot_times_r = multiplicationInterval(dot, r);
  const k_sqrt = sqrtInterval(k);
  const t = additionInterval(dot_times_r, k_sqrt); // t = r * dot(i, s) + sqrt(k)

  const result = runBinaryToIntervalOpComponentWise(
    multiplyVectorByScalar(i, r),
    multiplyVectorByScalar(s, t),
    SubtractionIntervalOp
  ); // (i * r) - (s * t)
  return result;
}

const RemainderIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    // x % y = x - y * trunc(x/y)
    return subtractionInterval(x, multiplicationInterval(y, truncInterval(divisionInterval(x, y))));
  },
};

/** Calculate an acceptance interval for x % y */
export function remainderInterval(x: number, y: number): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), RemainderIntervalOp);
}

const RoundIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    const k = Math.floor(n);
    const diff_before = n - k;
    const diff_after = k + 1 - n;
    if (diff_before < diff_after) {
      return correctlyRoundedInterval(k);
    } else if (diff_before > diff_after) {
      return correctlyRoundedInterval(k + 1);
    }

    // n is in the middle of two integers.
    // The tie breaking rule is 'k if k is even, k + 1 if k is odd'
    if (k % 2 === 0) {
      return correctlyRoundedInterval(k);
    }
    return correctlyRoundedInterval(k + 1);
  },
};

/** Calculate an acceptance interval of round(x) */
export function roundInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), RoundIntervalOp);
}

/**
 * Calculate an acceptance interval of saturate(n) as clamp(n, 0.0, 1.0)
 *
 * The definition of saturate is such that both possible implementations of
 * clamp will return the same value, so arbitrarily picking the minmax version
 * to use.
 */
export function saturateInterval(n: number): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(n),
    toF32Interval(0.0),
    toF32Interval(1.0),
    ClampMinMaxIntervalOp
  );
}

const SignIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    if (n > 0.0) {
      return correctlyRoundedInterval(1.0);
    }
    if (n < 0.0) {
      return correctlyRoundedInterval(-1.0);
    }

    return correctlyRoundedInterval(0.0);
  },
};

/** Calculate an acceptance interval of sin(x) */
export function signInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), SignIntervalOp);
}

const SinIntervalOp: PointToIntervalOp = {
  impl: limitPointToIntervalDomain(
    kNegPiToPiInterval,
    (n: number): F32Interval => {
      return absoluteErrorInterval(Math.sin(n), 2 ** -11);
    }
  ),
};

/** Calculate an acceptance interval of sin(x) */
export function sinInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), SinIntervalOp);
}

const SinhIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    // sinh(x) = (exp(x) - exp(-x)) * 0.5
    const minus_n = negationInterval(n);
    return multiplicationInterval(subtractionInterval(expInterval(n), expInterval(minus_n)), 0.5);
  },
};

/** Calculate an acceptance interval of sinh(x) */
export function sinhInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), SinhIntervalOp);
}

const SmoothStepOp: TernaryToIntervalOp = {
  impl: (low: number, high: number, x: number): F32Interval => {
    // For clamp(foo, 0.0, 1.0) the different implementations of clamp provide
    // the same value, so arbitrarily picking the minmax version to use.
    // t = clamp((x - low) / (high - low), 0.0, 1.0)
    // prettier-ignore
    const t = clampMedianInterval(
      divisionInterval(
        subtractionInterval(x, low),
        subtractionInterval(high, low)),
      0.0,
      1.0);
    // Inherited from t * t * (3.0 - 2.0 * t)
    // prettier-ignore
    return multiplicationInterval(
      t,
      multiplicationInterval(t,
        subtractionInterval(3.0,
          multiplicationInterval(2.0, t))));
  },
};

/** Calculate an acceptance interval of smoothStep(low, high, x) */
export function smoothStepInterval(low: number, high: number, x: number): F32Interval {
  return runTernaryToIntervalOp(
    toF32Interval(low),
    toF32Interval(high),
    toF32Interval(x),
    SmoothStepOp
  );
}

const SqrtIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return divisionInterval(1.0, inverseSqrtInterval(n));
  },
};

/** Calculate an acceptance interval of sqrt(x) */
export function sqrtInterval(n: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), SqrtIntervalOp);
}

const StepIntervalOp: BinaryToIntervalOp = {
  impl: (edge: number, x: number): F32Interval => {
    if (edge <= x) {
      return correctlyRoundedInterval(1.0);
    }
    return correctlyRoundedInterval(0.0);
  },
};

/** Calculate an acceptance 'interval' for step(edge, x)
 *
 * step only returns two possible values, so its interval requires special
 * interpretation in CTS tests.
 * This interval will be one of four values: [0, 0], [0, 1], [1, 1] & [-∞, +∞].
 * [0, 0] and [1, 1] indicate that the correct answer in point they encapsulate.
 * [0, 1] should not be treated as a span, i.e. 0.1 is acceptable, but instead
 * indicate either 0.0 or 1.0 are acceptable answers.
 * [-∞, +∞] is treated as the any interval, since an undefined or infinite value was passed in.
 */
export function stepInterval(edge: number, x: number): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(edge), toF32Interval(x), StepIntervalOp);
}

const SubtractionIntervalOp: BinaryToIntervalOp = {
  impl: (x: number, y: number): F32Interval => {
    return correctlyRoundedInterval(x - y);
  },
};

/** Calculate an acceptance interval of x - y */
export function subtractionInterval(x: number | F32Interval, y: number | F32Interval): F32Interval {
  return runBinaryToIntervalOp(toF32Interval(x), toF32Interval(y), SubtractionIntervalOp);
}

const TanIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return divisionInterval(sinInterval(n), cosInterval(n));
  },
};

/** Calculate an acceptance interval of tan(x) */
export function tanInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), TanIntervalOp);
}

const TanhIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return divisionInterval(sinhInterval(n), coshInterval(n));
  },
};

/** Calculate an acceptance interval of tanh(x) */
export function tanhInterval(n: number): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), TanhIntervalOp);
}

const TruncIntervalOp: PointToIntervalOp = {
  impl: (n: number): F32Interval => {
    return correctlyRoundedInterval(Math.trunc(n));
  },
};

/** Calculate an acceptance interval of trunc(x) */
export function truncInterval(n: number | F32Interval): F32Interval {
  return runPointToIntervalOp(toF32Interval(n), TruncIntervalOp);
}

/**
 * Once-allocated ArrayBuffer/views to avoid overhead of allocation when converting between numeric formats
 *
 * unpackData* is shared between all of the unpack*Interval functions, so to avoid re-entrancy problems, they should
 * not call each other or themselves directly or indirectly.
 */
const unpackData = new ArrayBuffer(4);
const unpackDataU32 = new Uint32Array(unpackData);
const unpackDataU16 = new Uint16Array(unpackData);
const unpackDataU8 = new Uint8Array(unpackData);
const unpackDataI16 = new Int16Array(unpackData);
const unpackDataI8 = new Int8Array(unpackData);
const unpackDataF16 = new Float16Array(unpackData);

/** Calculate an acceptance interval vector for unpack2x16float(x) */
export function unpack2x16floatInterval(n: number): F32Vector {
  assert(
    n >= kValue.u32.min && n <= kValue.u32.max,
    'unpack2x16floatInterval only accepts values on the bounds of u32'
  );
  unpackDataU32[0] = n;
  if (unpackDataF16.some(f => !isFiniteF16(f))) {
    return [F32Interval.any(), F32Interval.any()];
  }

  const result: F32Vector = [
    quantizeToF16Interval(unpackDataF16[0]),
    quantizeToF16Interval(unpackDataF16[1]),
  ];

  if (result.some(r => !r.isFinite())) {
    return [F32Interval.any(), F32Interval.any()];
  }
  return result;
}

const Unpack2x16snormIntervalOp = (n: number): F32Interval => {
  return maxInterval(divisionInterval(n, 32767), -1);
};

/** Calculate an acceptance interval vector for unpack2x16snorm(x) */
export function unpack2x16snormInterval(n: number): F32Vector {
  assert(
    n >= kValue.u32.min && n <= kValue.u32.max,
    'unpack2x16snormInterval only accepts values on the bounds of u32'
  );
  unpackDataU32[0] = n;
  return [Unpack2x16snormIntervalOp(unpackDataI16[0]), Unpack2x16snormIntervalOp(unpackDataI16[1])];
}

const Unpack2x16unormIntervalOp = (n: number): F32Interval => {
  return divisionInterval(n, 65535);
};

/** Calculate an acceptance interval vector for unpack2x16unorm(x) */
export function unpack2x16unormInterval(n: number): F32Vector {
  assert(
    n >= kValue.u32.min && n <= kValue.u32.max,
    'unpack2x16unormInterval only accepts values on the bounds of u32'
  );
  unpackDataU32[0] = n;
  return [Unpack2x16unormIntervalOp(unpackDataU16[0]), Unpack2x16unormIntervalOp(unpackDataU16[1])];
}

const Unpack4x8snormIntervalOp = (n: number): F32Interval => {
  return maxInterval(divisionInterval(n, 127), -1);
};

/** Calculate an acceptance interval vector for unpack4x8snorm(x) */
export function unpack4x8snormInterval(n: number): F32Vector {
  assert(
    n >= kValue.u32.min && n <= kValue.u32.max,
    'unpack4x8snormInterval only accepts values on the bounds of u32'
  );
  unpackDataU32[0] = n;
  return [
    Unpack4x8snormIntervalOp(unpackDataI8[0]),
    Unpack4x8snormIntervalOp(unpackDataI8[1]),
    Unpack4x8snormIntervalOp(unpackDataI8[2]),
    Unpack4x8snormIntervalOp(unpackDataI8[3]),
  ];
}

const Unpack4x8unormIntervalOp = (n: number): F32Interval => {
  return divisionInterval(n, 255);
};

/** Calculate an acceptance interval vector for unpack4x8unorm(x) */
export function unpack4x8unormInterval(n: number): F32Vector {
  assert(
    n >= kValue.u32.min && n <= kValue.u32.max,
    'unpack4x8unormInterval only accepts values on the bounds of u32'
  );
  unpackDataU32[0] = n;
  return [
    Unpack4x8unormIntervalOp(unpackDataU8[0]),
    Unpack4x8unormIntervalOp(unpackDataU8[1]),
    Unpack4x8unormIntervalOp(unpackDataU8[2]),
    Unpack4x8unormIntervalOp(unpackDataU8[3]),
  ];
}
