import { assert, unreachable } from '../../common/util/util.js';
import { Float16Array } from '../../external/petamoriken/float16/float16.js';
import { Case, IntervalFilter } from '../shader/execution/expression/expression.js';

import { anyOf } from './compare.js';
import { kValue } from './constants.js';
import {
  abstractFloat,
  f16,
  f32,
  isFloatType,
  reinterpretF16AsU16,
  reinterpretF32AsU32,
  reinterpretF64AsU32s,
  reinterpretU16AsF16,
  reinterpretU32AsF32,
  reinterpretU32sAsF64,
  Scalar,
  ScalarType,
  toMatrix,
  toVector,
  u32,
} from './conversion.js';
import {
  calculatePermutations,
  cartesianProduct,
  correctlyRoundedF16,
  correctlyRoundedF32,
  correctlyRoundedF64,
  flatten2DArray,
  FlushMode,
  flushSubnormalNumberF16,
  flushSubnormalNumberF32,
  flushSubnormalNumberF64,
  isFiniteF16,
  isFiniteF32,
  isSubnormalNumberF16,
  isSubnormalNumberF32,
  isSubnormalNumberF64,
  map2DArray,
  oneULPF16,
  oneULPF32,
  oneULPF64,
  quantizeToF32,
  quantizeToF16,
  unflatten2DArray,
} from './math.js';

/** Indicate the kind of WGSL floating point numbers being operated on */
export type FPKind = 'f32' | 'f16' | 'abstract';

// Containers

/**
 * Representation of bounds for an interval as an array with either one or two
 * elements. Single element indicates that the interval is a single point. For
 * two elements, the first is the lower bound of the interval and the second is
 * the upper bound.
 */
export type IntervalBounds = [number] | [number, number];

/** Represents a closed interval of floating point numbers */
export class FPInterval {
  public readonly kind: FPKind;
  public readonly begin: number;
  public readonly end: number;

  /**
   * Constructor
   *
   * `FPTraits.toInterval` is the preferred way to create FPIntervals
   *
   * @param kind the floating point number type this is an interval for
   * @param bounds beginning and end of the interval
   */
  public constructor(kind: FPKind, ...bounds: IntervalBounds) {
    this.kind = kind;

    const [begin, end] = bounds.length === 2 ? bounds : [bounds[0], bounds[0]];
    assert(!Number.isNaN(begin) && !Number.isNaN(end), `bounds need to be non-NaN`);
    assert(begin <= end, `bounds[0] (${begin}) must be less than or equal to bounds[1]  (${end})`);

    this.begin = begin;
    this.end = end;
  }

  /** @returns the floating point traits for this interval */
  public traits(): FPTraits {
    return FP[this.kind];
  }

  /** @returns begin and end if non-point interval, otherwise just begin */
  public bounds(): IntervalBounds {
    return this.isPoint() ? [this.begin] : [this.begin, this.end];
  }

  /** @returns if a point or interval is completely contained by this interval */
  public contains(n: number | FPInterval): boolean {
    if (Number.isNaN(n)) {
      // Being the 'any' interval indicates that accuracy is not defined for this
      // test, so the test is just checking that this input doesn't cause the
      // implementation to misbehave, so NaN is accepted.
      return this.begin === Number.NEGATIVE_INFINITY && this.end === Number.POSITIVE_INFINITY;
    }

    if (n instanceof FPInterval) {
      return this.begin <= n.begin && this.end >= n.end;
    }
    return this.begin <= n && this.end >= n;
  }

  /** @returns if any values in the interval may be flushed to zero, this
   *           includes any subnormals and zero itself.
   */
  public containsZeroOrSubnormals(): boolean {
    return !(
      this.end < this.traits().constants().negative.subnormal.min ||
      this.begin > this.traits().constants().positive.subnormal.max
    );
  }

  /** @returns if this interval contains a single point */
  public isPoint(): boolean {
    return this.begin === this.end;
  }

  /** @returns if this interval only contains finite values */
  public isFinite(): boolean {
    return this.traits().isFinite(this.begin) && this.traits().isFinite(this.end);
  }

  /** @returns a string representation for logging purposes */
  public toString(): string {
    return `{ '${this.kind}', [${this.bounds().map(this.traits().scalarBuilder)}] }`;
  }
}

/**
 * SerializedFPInterval holds the serialized form of a FPInterval.
 * This form can be safely encoded to JSON.
 */
export type SerializedFPInterval =
  | { kind: 'f32'; unbounded: false; begin: number; end: number }
  | { kind: 'f32'; unbounded: true }
  | { kind: 'f16'; unbounded: false; begin: number; end: number }
  | { kind: 'f16'; unbounded: true }
  | { kind: 'abstract'; unbounded: false; begin: [number, number]; end: [number, number] }
  | { kind: 'abstract'; unbounded: true };

/** serializeFPInterval() converts a FPInterval to a SerializedFPInterval */
export function serializeFPInterval(i: FPInterval): SerializedFPInterval {
  const traits = FP[i.kind];
  switch (i.kind) {
    case 'abstract': {
      if (i === traits.constants().unboundedInterval) {
        return { kind: 'abstract', unbounded: true };
      } else {
        return {
          kind: 'abstract',
          unbounded: false,
          begin: reinterpretF64AsU32s(i.begin),
          end: reinterpretF64AsU32s(i.end),
        };
      }
    }
    case 'f32': {
      if (i === traits.constants().unboundedInterval) {
        return { kind: 'f32', unbounded: true };
      } else {
        return {
          kind: 'f32',
          unbounded: false,
          begin: reinterpretF32AsU32(i.begin),
          end: reinterpretF32AsU32(i.end),
        };
      }
    }
    case 'f16': {
      if (i === traits.constants().unboundedInterval) {
        return { kind: 'f16', unbounded: true };
      } else {
        return {
          kind: 'f16',
          unbounded: false,
          begin: reinterpretF16AsU16(i.begin),
          end: reinterpretF16AsU16(i.end),
        };
      }
    }
  }
  unreachable(`Unable to serialize FPInterval ${i}`);
}

/** serializeFPInterval() converts a SerializedFPInterval to a FPInterval */
export function deserializeFPInterval(data: SerializedFPInterval): FPInterval {
  const kind = data.kind;
  const traits = FP[kind];
  if (data.unbounded) {
    return traits.constants().unboundedInterval;
  }
  switch (kind) {
    case 'abstract': {
      return traits.toInterval([reinterpretU32sAsF64(data.begin), reinterpretU32sAsF64(data.end)]);
    }
    case 'f32': {
      return traits.toInterval([reinterpretU32AsF32(data.begin), reinterpretU32AsF32(data.end)]);
    }
    case 'f16': {
      return traits.toInterval([reinterpretU16AsF16(data.begin), reinterpretU16AsF16(data.end)]);
    }
  }
  unreachable(`Unable to deserialize data ${data}`);
}

/**
 * Representation of a vec2/3/4 of floating point intervals as an array of
 * FPIntervals.
 */
export type FPVector =
  | [FPInterval, FPInterval]
  | [FPInterval, FPInterval, FPInterval]
  | [FPInterval, FPInterval, FPInterval, FPInterval];

/** Shorthand for an Array of Arrays that contains a column-major matrix */
type Array2D<T> = T[][];

/**
 * Representation of a matCxR of floating point intervals as an array of arrays
 * of FPIntervals. This maps onto the WGSL concept of matrix. Internally
 */
export type FPMatrix =
  | [[FPInterval, FPInterval], [FPInterval, FPInterval]]
  | [[FPInterval, FPInterval], [FPInterval, FPInterval], [FPInterval, FPInterval]]
  | [
      [FPInterval, FPInterval],
      [FPInterval, FPInterval],
      [FPInterval, FPInterval],
      [FPInterval, FPInterval]
    ]
  | [[FPInterval, FPInterval, FPInterval], [FPInterval, FPInterval, FPInterval]]
  | [
      [FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval]
    ]
  | [
      [FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval]
    ]
  | [
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval]
    ]
  | [
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval]
    ]
  | [
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval],
      [FPInterval, FPInterval, FPInterval, FPInterval]
    ];

// Utilities

/** @returns input with an appended 0, if inputs contains non-zero subnormals */
// When f16 traits is defined, this can be replaced with something like
// `FP.f16..addFlushIfNeeded`
function addFlushedIfNeededF16(values: number[]): number[] {
  return values.some(v => v !== 0 && isSubnormalNumberF16(v)) ? values.concat(0) : values;
}

// Operations

/**
 * A function that converts a point to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface ScalarToInterval {
  (x: number): FPInterval;
}

/** Operation used to implement a ScalarToInterval */
interface ScalarToIntervalOp {
  /** @returns acceptance interval for a function at point x */
  impl: ScalarToInterval;

  /**
   * Calculates where in the domain defined by x the min/max extrema of impl
   * occur and returns a span of those points to be used as the domain instead.
   *
   * Used by this.runScalarToIntervalOp before invoking impl.
   * If not defined, the bounds of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for operations that meet all the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   *      i.e. fooInterval takes in x: number | FPInterval, not x: number
   */
  extrema?: (x: FPInterval) => FPInterval;
}

/**
 * A function that converts a pair of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface ScalarPairToInterval {
  (x: number, y: number): FPInterval;
}

/** Operation used to implement a ScalarPairToInterval */
interface ScalarPairToIntervalOp {
  /** @returns acceptance interval for a function at point (x, y) */
  impl: ScalarPairToInterval;
  /**
   * Calculates where in domain defined by x & y the min/max extrema of impl
   * occur and returns spans of those points to be used as the domain instead.
   *
   * Used by runScalarPairToIntervalOp before invoking impl.
   * If not defined, the bounds of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for functions that meet all of the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   */
  extrema?: (x: FPInterval, y: FPInterval) => [FPInterval, FPInterval];
}

/** Domain for a ScalarPairToInterval implementation */
interface ScalarPairToIntervalDomain {
  // Arrays to support discrete valid domain intervals
  x: FPInterval[];
  y: FPInterval[];
}

/**
 * A function that converts a triplet of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface ScalarTripleToInterval {
  (x: number, y: number, z: number): FPInterval;
}

/** Operation used to implement a ScalarTripleToInterval */
interface ScalarTripleToIntervalOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns acceptance interval for a function at point (x, y, z) */
  impl: ScalarTripleToInterval;
}

// Currently ScalarToVector is not integrated with the rest of the floating point
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
export interface ScalarToVector {
  (n: number): FPVector;
}

/**
 * A function that converts a vector to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorToInterval {
  (x: number[]): FPInterval;
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
  (x: number[], y: number[]): FPInterval;
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
  (x: number[]): FPVector;
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
  (x: number[], y: number[]): FPVector;
}

/** Operation used to implement a VectorPairToVector */
interface VectorPairToVectorOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns a vector of acceptance intervals for a function on vectors (x, y) */
  impl: VectorPairToVector;
}

/**
 * A function that converts a vector and a scalar to a vector of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorScalarToVector {
  (x: number[], y: number): FPVector;
}

/**
 * A function that converts a scalar and a vector  to a vector of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface ScalarVectorToVector {
  (x: number, y: number[]): FPVector;
}

/**
 * A function that converts a matrix to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface MatrixToScalar {
  (m: Array2D<number>): FPInterval;
}

/** Operation used to implement a MatrixToMatrix */
interface MatrixToMatrixOp {
  // Re-using the *Op interface pattern for symmetry with the other operations.
  /** @returns a matrix of acceptance intervals for a function on matrix x */
  impl: MatrixToMatrix;
}

/**
 * A function that converts a matrix to a matrix of acceptance intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface MatrixToMatrix {
  (m: Array2D<number>): FPMatrix;
}

/**
 * A function that converts a pair of matrices to a matrix of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface MatrixPairToMatrix {
  (x: Array2D<number>, y: Array2D<number>): FPMatrix;
}

/**
 * A function that converts a matrix and a scalar to a matrix of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface MatrixScalarToMatrix {
  (x: Array2D<number>, y: number): FPMatrix;
}

/**
 * A function that converts a scalar and a matrix to a matrix of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface ScalarMatrixToMatrix {
  (x: number, y: Array2D<number>): FPMatrix;
}

/**
 * A function that converts a matrix and a vector to a vector of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface MatrixVectorToVector {
  (x: Array2D<number>, y: number[]): FPVector;
}

/**
 * A function that converts a vector and a matrix to a vector of acceptance
 * intervals.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface VectorMatrixToVector {
  (x: number[], y: Array2D<number>): FPVector;
}

// Traits

/**
 * Typed structure containing all the limits/constants defined for each
 * WGSL floating point kind
 */
interface FPConstants {
  positive: {
    min: number;
    max: number;
    infinity: number;
    nearest_max: number;
    less_than_one: number;
    subnormal: {
      min: number;
      max: number;
    };
    pi: {
      whole: number;
      three_quarters: number;
      half: number;
      third: number;
      quarter: number;
      sixth: number;
    };
    e: number;
  };
  negative: {
    min: number;
    max: number;
    infinity: number;
    nearest_min: number;
    less_than_one: number;
    subnormal: {
      min: number;
      max: number;
    };
    pi: {
      whole: number;
      three_quarters: number;
      half: number;
      third: number;
      quarter: number;
      sixth: number;
    };
  };
  unboundedInterval: FPInterval;
  zeroInterval: FPInterval;
  negPiToPiInterval: FPInterval;
  greaterThanZeroInterval: FPInterval;
  zeroVector: {
    2: FPVector;
    3: FPVector;
    4: FPVector;
  };
  unboundedVector: {
    2: FPVector;
    3: FPVector;
    4: FPVector;
  };
  unboundedMatrix: {
    2: {
      2: FPMatrix;
      3: FPMatrix;
      4: FPMatrix;
    };
    3: {
      2: FPMatrix;
      3: FPMatrix;
      4: FPMatrix;
    };
    4: {
      2: FPMatrix;
      3: FPMatrix;
      4: FPMatrix;
    };
  };
}

/** A representation of an FPInterval for a case param */
export type FPIntervalParam = {
  kind: FPKind;
  interval: number | IntervalBounds;
};

/** Abstract base class for all floating-point traits */
export abstract class FPTraits {
  public readonly kind: FPKind;
  protected constructor(k: FPKind) {
    this.kind = k;
  }

  public abstract constants(): FPConstants;

  // Utilities - Implemented
  /** @returns an interval containing the point or the original interval */
  public toInterval(n: number | IntervalBounds | FPInterval): FPInterval {
    if (n instanceof FPInterval) {
      if (n.kind === this.kind) {
        return n;
      }
      return new FPInterval(this.kind, ...n.bounds());
    }

    if (n instanceof Array) {
      return new FPInterval(this.kind, ...n);
    }

    return new FPInterval(this.kind, n, n);
  }

  /**
   * Makes a param that can be turned into an interval
   */
  public toParam(n: number | IntervalBounds): FPIntervalParam {
    return {
      kind: this.kind,
      interval: n,
    };
  }

  /**
   * Converts p into an FPInterval if it is an FPIntervalPAram
   */
  public fromParam(
    p: number | IntervalBounds | FPIntervalParam
  ): number | IntervalBounds | FPInterval {
    const param = p as FPIntervalParam;
    if (param.interval && param.kind) {
      assert(param.kind === this.kind);
      return this.toInterval(param.interval);
    }
    return p as number | IntervalBounds;
  }

  /**
   * @returns an interval with the tightest bounds that includes all provided
   *          intervals
   */
  public spanIntervals(...intervals: FPInterval[]): FPInterval {
    assert(intervals.length > 0, `span of an empty list of FPIntervals is not allowed`);
    assert(
      intervals.every(i => i.kind === this.kind),
      `span is only defined for intervals with the same kind`
    );
    let begin = Number.POSITIVE_INFINITY;
    let end = Number.NEGATIVE_INFINITY;
    intervals.forEach(i => {
      begin = Math.min(i.begin, begin);
      end = Math.max(i.end, end);
    });
    return this.toInterval([begin, end]);
  }

  /** Narrow an array of values to FPVector if possible */
  public isVector(v: (number | IntervalBounds | FPInterval)[]): v is FPVector {
    if (v.every(e => e instanceof FPInterval && e.kind === this.kind)) {
      return v.length === 2 || v.length === 3 || v.length === 4;
    }
    return false;
  }

  /** @returns an FPVector representation of an array of values if possible */
  public toVector(v: (number | IntervalBounds | FPInterval)[]): FPVector {
    if (this.isVector(v)) {
      return v;
    }

    const f = v.map(e => this.toInterval(e));
    // The return of the map above is a FPInterval[], which needs to be narrowed
    // to FPVector, since FPVector is defined as fixed length tuples.
    if (this.isVector(f)) {
      return f;
    }
    unreachable(`Cannot convert [${v}] to FPVector`);
  }

  /**
   * @returns a FPVector where each element is the span for corresponding
   *          elements at the same index in the input vectors
   */
  public spanVectors(...vectors: FPVector[]): FPVector {
    assert(
      vectors.every(e => this.isVector(e)),
      'Vector span is not defined for vectors of differing floating point kinds'
    );

    const vector_length = vectors[0].length;
    assert(
      vectors.every(e => e.length === vector_length),
      `Vector span is not defined for vectors of differing lengths`
    );

    const result: FPInterval[] = new Array<FPInterval>(vector_length);

    for (let i = 0; i < vector_length; i++) {
      result[i] = this.spanIntervals(...vectors.map(v => v[i]));
    }
    return this.toVector(result);
  }

  /** Narrow an array of an array of values to FPMatrix if possible */
  public isMatrix(m: Array2D<number | IntervalBounds | FPInterval> | FPVector[]): m is FPMatrix {
    if (!m.every(c => c.every(e => e instanceof FPInterval && e.kind === this.kind))) {
      return false;
    }
    // At this point m guaranteed to be a FPInterval[][], but maybe typed as a
    // FPVector[].
    // Coercing the type since FPVector[] is functionally equivalent to
    // FPInterval[][] for .length and .every, but they are type compatible,
    // since tuples are not equivalent to arrays, so TS considers c in .every to
    // be unresolvable below, even though our usage is safe.
    m = m as FPInterval[][];

    if (m.length > 4 || m.length < 2) {
      return false;
    }

    const num_rows = m[0].length;
    if (num_rows > 4 || num_rows < 2) {
      return false;
    }

    return m.every(c => c.length === num_rows);
  }

  /** @returns an FPMatrix representation of an array of an array of values if possible */
  public toMatrix(m: Array2D<number | IntervalBounds | FPInterval> | FPVector[]): FPMatrix {
    if (this.isMatrix(m)) {
      return m;
    }

    const result = map2DArray(m, this.toInterval.bind(this));

    // The return of the map above is a FPInterval[][], which needs to be
    // narrowed to FPMatrix, since FPMatrix is defined as fixed length tuples.
    if (this.isMatrix(result)) {
      return result;
    }
    unreachable(`Cannot convert ${m} to FPMatrix`);
  }

  /**
   * @returns a FPMatrix where each element is the span for corresponding
   *          elements at the same index in the input matrices
   */
  public spanMatrices(...matrices: FPMatrix[]): FPMatrix {
    // Coercing the type of matrices, since tuples are not generally compatible
    // with Arrays, but they are functionally equivalent for the usages in this
    // function.
    const ms = matrices as Array2D<FPInterval>[];
    const num_cols = ms[0].length;
    const num_rows = ms[0][0].length;
    assert(
      ms.every(m => m.length === num_cols && m.every(r => r.length === num_rows)),
      `Matrix span is not defined for Matrices of differing dimensions`
    );

    const result: Array2D<FPInterval> = [...Array(num_cols)].map(_ => [...Array(num_rows)]);
    for (let i = 0; i < num_cols; i++) {
      for (let j = 0; j < num_rows; j++) {
        result[i][j] = this.spanIntervals(...ms.map(m => m[i][j]));
      }
    }

    return this.toMatrix(result);
  }

  /** @returns input with an appended 0, if inputs contains non-zero subnormals */
  public addFlushedIfNeeded(values: number[]): number[] {
    const subnormals = values.filter(this.isSubnormal);
    const needs_zero = subnormals.length > 0 && subnormals.every(s => s !== 0);
    return needs_zero ? values.concat(0) : values;
  }

  /**
   * Restrict the inputs to an ScalarToInterval operation
   *
   * Only used for operations that have tighter domain requirements than 'must
   * be finite'.
   *
   * @param domain interval to restrict inputs to
   * @param impl operation implementation to run if input is within the required domain
   * @returns a ScalarToInterval that calls impl if domain contains the input,
   *          otherwise it returns an unbounded interval */
  protected limitScalarToIntervalDomain(
    domain: FPInterval,
    impl: ScalarToInterval
  ): ScalarToInterval {
    return (n: number): FPInterval => {
      return domain.contains(n) ? impl(n) : this.constants().unboundedInterval;
    };
  }

  /**
   * Restrict the inputs to a ScalarPairToInterval
   *
   * Only used for operations that have tighter domain requirements than 'must be
   * finite'.
   *
   * @param domain set of intervals to restrict inputs to
   * @param impl operation implementation to run if input is within the required domain
   * @returns a ScalarPairToInterval that calls impl if domain contains the input,
   *          otherwise it returns an unbounded interval */
  protected limitScalarPairToIntervalDomain(
    domain: ScalarPairToIntervalDomain,
    impl: ScalarPairToInterval
  ): ScalarPairToInterval {
    return (x: number, y: number): FPInterval => {
      if (!domain.x.some(d => d.contains(x)) || !domain.y.some(d => d.contains(y))) {
        return this.constants().unboundedInterval;
      }

      return impl(x, y);
    };
  }

  /** Stub for scalar to interval generator */
  protected unimplementedScalarToInterval(_x: number | FPInterval): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for scalar pair to interval generator */
  protected unimplementedScalarPairToInterval(
    _x: number | FPInterval,
    _y: number | FPInterval
  ): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for scalar triple to interval generator */
  protected unimplementedScalarTripleToInterval(
    _x: number | FPInterval,
    _y: number | FPInterval,
    _z: number | FPInterval
  ): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for scalar to vector generator */
  protected unimplementedScalarToVector(_x: number | FPInterval): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector to interval generator */
  protected unimplementedVectorToInterval(_x: (number | FPInterval)[]): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector pair to interval generator */
  protected unimplementedVectorPairToInterval(
    _x: (number | FPInterval)[],
    _y: (number | FPInterval)[]
  ): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector to vector generator */
  protected unimplementedVectorToVector(_x: (number | FPInterval)[]): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector pair to vector generator */
  protected unimplementedVectorPairToVector(
    _x: (number | FPInterval)[],
    _y: (number | FPInterval)[]
  ): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector-scalar to vector generator */
  protected unimplementedVectorScalarToVector(
    _x: (number | FPInterval)[],
    _y: number | FPInterval
  ): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for scalar-vector to vector generator */
  protected unimplementedScalarVectorToVector(
    _x: number | FPInterval,
    _y: (number | FPInterval)[]
  ): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for matrix to interval generator */
  protected unimplementedMatrixToInterval(_x: Array2D<number>): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for matrix to matix generator */
  protected unimplementedMatrixToMatrix(_x: Array2D<number>): FPMatrix {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for matrix pair to matrix generator */
  protected unimplementedMatrixPairToMatrix(_x: Array2D<number>, _y: Array2D<number>): FPMatrix {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for matrix-scalar to matrix generator  */
  protected unimplementedMatrixScalarToMatrix(
    _x: Array2D<number>,
    _y: number | FPInterval
  ): FPMatrix {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for scalar-matrix to matrix generator  */
  protected unimplementedScalarMatrixToMatrix(
    _x: number | FPInterval,
    _y: Array2D<number>
  ): FPMatrix {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for matrix-vector to vector generator  */
  protected unimplementedMatrixVectorToVector(
    _x: Array2D<number>,
    _y: (number | FPInterval)[]
  ): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for vector-matrix to vector generator  */
  protected unimplementedVectorMatrixToVector(
    _x: (number | FPInterval)[],
    _y: Array2D<number>
  ): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for distance generator */
  protected unimplementedDistance(_x: number | number[], _y: number | number[]): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for faceForward */
  protected unimplementedFaceForward(
    _x: number[],
    _y: number[],
    _z: number[]
  ): (FPVector | undefined)[] {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for length generator */
  protected unimplementedLength(_x: number | FPInterval | number[] | FPVector): FPInterval {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for modf generator */
  protected unimplementedModf(_x: number): { fract: FPInterval; whole: FPInterval } {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Stub for refract generator */
  protected unimplementedRefract(_i: number[], _s: number[], _r: number): FPVector {
    unreachable(`Not yet implemented for ${this.kind}`);
  }

  /** Version of absoluteErrorInterval that always returns the unboundedInterval */
  protected unboundedAbsoluteErrorInterval(_n: number, _error_range: number): FPInterval {
    return this.constants().unboundedInterval;
  }

  /** Version of ulpInterval that always returns the unboundedInterval */
  protected unboundedUlpInterval(_n: number, _numULP: number): FPInterval {
    return this.constants().unboundedInterval;
  }

  // Utilities - Defined by subclass
  /**
   * @returns the nearest precise value to the input. Rounding should be IEEE
   *          'roundTiesToEven'.
   */
  public abstract readonly quantize: (n: number) => number;
  /** @returns all valid roundings of input */
  public abstract readonly correctlyRounded: (n: number) => number[];
  /** @returns true if input is considered finite, otherwise false */
  public abstract readonly isFinite: (n: number) => boolean;
  /** @returns true if input is considered subnormal, otherwise false */
  public abstract readonly isSubnormal: (n: number) => boolean;
  /** @returns 0 if the provided number is subnormal, otherwise returns the proved number */
  public abstract readonly flushSubnormal: (n: number) => number;
  /** @returns 1 * ULP: (number) */
  public abstract readonly oneULP: (target: number, mode?: FlushMode) => number;
  /** @returns a builder for converting numbers to Scalars */
  public abstract readonly scalarBuilder: (n: number) => Scalar;

  // Framework - Cases

  /**
   * @returns a Case for the param and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeScalarToIntervalCase(
    param: number,
    filter: IntervalFilter,
    ...ops: ScalarToInterval[]
  ): Case | undefined {
    param = this.quantize(param);

    const intervals = ops.map(o => o(param));
    if (filter === 'finite' && intervals.some(i => !i.isFinite())) {
      return undefined;
    }
    return { input: [this.scalarBuilder(param)], expected: anyOf(...intervals) };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateScalarToIntervalCases(
    params: number[],
    filter: IntervalFilter,
    ...ops: ScalarToInterval[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeScalarToIntervalCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param0 the first param to pass in
   * @param param1 the second param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeScalarPairToIntervalCase(
    param0: number,
    param1: number,
    filter: IntervalFilter,
    ...ops: ScalarPairToInterval[]
  ): Case | undefined {
    param0 = this.quantize(param0);
    param1 = this.quantize(param1);

    const intervals = ops.map(o => o(param0, param1));
    if (filter === 'finite' && intervals.some(i => !i.isFinite())) {
      return undefined;
    }
    return {
      input: [this.scalarBuilder(param0), this.scalarBuilder(param1)],
      expected: anyOf(...intervals),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param param0s array of inputs to try for the first input
   * @param param1s array of inputs to try for the second input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateScalarPairToIntervalCases(
    param0s: number[],
    param1s: number[],
    filter: IntervalFilter,
    ...ops: ScalarPairToInterval[]
  ): Case[] {
    return cartesianProduct(param0s, param1s).reduce((cases, e) => {
      const c = this.makeScalarPairToIntervalCase(e[0], e[1], filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param0 the first param to pass in
   * @param param1 the second param to pass in
   * @param param2 the third param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public makeScalarTripleToIntervalCase(
    param0: number,
    param1: number,
    param2: number,
    filter: IntervalFilter,
    ...ops: ScalarTripleToInterval[]
  ): Case | undefined {
    param0 = this.quantize(param0);
    param1 = this.quantize(param1);
    param2 = this.quantize(param2);

    const intervals = ops.map(o => o(param0, param1, param2));
    if (filter === 'finite' && intervals.some(i => !i.isFinite())) {
      return undefined;
    }
    return {
      input: [this.scalarBuilder(param0), this.scalarBuilder(param1), this.scalarBuilder(param2)],
      expected: anyOf(...intervals),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param param0s array of inputs to try for the first input
   * @param param1s array of inputs to try for the second input
   * @param param2s array of inputs to try for the third input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateScalarTripleToIntervalCases(
    param0s: number[],
    param1s: number[],
    param2s: number[],
    filter: IntervalFilter,
    ...ops: ScalarTripleToInterval[]
  ): Case[] {
    return cartesianProduct(param0s, param1s, param2s).reduce((cases, e) => {
      const c = this.makeScalarTripleToIntervalCase(e[0], e[1], e[2], filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeVectorToIntervalCase(
    param: number[],
    filter: IntervalFilter,
    ...ops: VectorToInterval[]
  ): Case | undefined {
    param = param.map(this.quantize);

    const intervals = ops.map(o => o(param));
    if (filter === 'finite' && intervals.some(i => !i.isFinite())) {
      return undefined;
    }
    return {
      input: [toVector(param, this.scalarBuilder)],
      expected: anyOf(...intervals),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateVectorToIntervalCases(
    params: number[][],
    filter: IntervalFilter,
    ...ops: VectorToInterval[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeVectorToIntervalCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param0 the first param to pass in
   * @param param1 the second param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeVectorPairToIntervalCase(
    param0: number[],
    param1: number[],
    filter: IntervalFilter,
    ...ops: VectorPairToInterval[]
  ): Case | undefined {
    param0 = param0.map(this.quantize);
    param1 = param1.map(this.quantize);

    const intervals = ops.map(o => o(param0, param1));
    if (filter === 'finite' && intervals.some(i => !i.isFinite())) {
      return undefined;
    }
    return {
      input: [toVector(param0, this.scalarBuilder), toVector(param1, this.scalarBuilder)],
      expected: anyOf(...intervals),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param param0s array of inputs to try for the first input
   * @param param1s array of inputs to try for the second input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateVectorPairToIntervalCases(
    param0s: number[][],
    param1s: number[][],
    filter: IntervalFilter,
    ...ops: VectorPairToInterval[]
  ): Case[] {
    return cartesianProduct(param0s, param1s).reduce((cases, e) => {
      const c = this.makeVectorPairToIntervalCase(e[0], e[1], filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the param and vector of intervals generator provided
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals.
   */
  private makeVectorToVectorCase(
    param: number[],
    filter: IntervalFilter,
    ...ops: VectorToVector[]
  ): Case | undefined {
    param = param.map(this.quantize);

    const vectors = ops.map(o => o(param));
    if (filter === 'finite' && vectors.some(v => v.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [toVector(param, this.scalarBuilder)],
      expected: anyOf(...vectors),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals.
   */
  public generateVectorToVectorCases(
    params: number[][],
    filter: IntervalFilter,
    ...ops: VectorToVector[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeVectorToVectorCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and the interval vector generator provided.
   * The Case will use an interval comparator for matching results.
   * @param scalar the scalar param to pass in
   * @param vector the vector param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance intervals
   */
  private makeScalarVectorToVectorCase(
    scalar: number,
    vector: number[],
    filter: IntervalFilter,
    ...ops: ScalarVectorToVector[]
  ): Case | undefined {
    scalar = this.quantize(scalar);
    vector = vector.map(this.quantize);

    const results = ops.map(o => o(scalar, vector));
    if (filter === 'finite' && results.some(r => r.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [this.scalarBuilder(scalar), toVector(vector, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param scalars array of scalar inputs to try
   * @param vectors array of vector inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance intervals
   */
  public generateScalarVectorToVectorCases(
    scalars: number[],
    vectors: number[][],
    filter: IntervalFilter,
    ...ops: ScalarVectorToVector[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    scalars.forEach(scalar => {
      vectors.forEach(vector => {
        const c = this.makeScalarVectorToVectorCase(scalar, vector, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  /**
   * @returns a Case for the params and the interval vector generator provided.
   * The Case will use an interval comparator for matching results.
   * @param vector the vector param to pass in
   * @param scalar the scalar param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance intervals
   */
  private makeVectorScalarToVectorCase(
    vector: number[],
    scalar: number,
    filter: IntervalFilter,
    ...ops: VectorScalarToVector[]
  ): Case | undefined {
    vector = vector.map(this.quantize);
    scalar = this.quantize(scalar);

    const results = ops.map(o => o(vector, scalar));
    if (filter === 'finite' && results.some(r => r.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [toVector(vector, this.scalarBuilder), this.scalarBuilder(scalar)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param vectors array of vector inputs to try
   * @param scalars array of scalar inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance intervals
   */
  public generateVectorScalarToVectorCases(
    vectors: number[][],
    scalars: number[],
    filter: IntervalFilter,
    ...ops: VectorScalarToVector[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    vectors.forEach(vector => {
      scalars.forEach(scalar => {
        const c = this.makeVectorScalarToVectorCase(vector, scalar, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  /**
   * @returns a Case for the param and vector of intervals generator provided
   * @param param0 the first param to pass in
   * @param param1 the second param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals.
   */
  private makeVectorPairToVectorCase(
    param0: number[],
    param1: number[],
    filter: IntervalFilter,
    ...ops: VectorPairToVector[]
  ): Case | undefined {
    param0 = param0.map(this.quantize);
    param1 = param1.map(this.quantize);
    const vectors = ops.map(o => o(param0, param1));
    if (filter === 'finite' && vectors.some(v => v.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [toVector(param0, this.scalarBuilder), toVector(param1, this.scalarBuilder)],
      expected: anyOf(...vectors),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param param0s array of inputs to try for the first input
   * @param param1s array of inputs to try for the second input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals.
   */
  public generateVectorPairToVectorCases(
    param0s: number[][],
    param1s: number[][],
    filter: IntervalFilter,
    ...ops: VectorPairToVector[]
  ): Case[] {
    return cartesianProduct(param0s, param1s).reduce((cases, e) => {
      const c = this.makeVectorPairToVectorCase(e[0], e[1], filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the param and an array of interval generators provided
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeMatrixToScalarCase(
    param: number[][],
    filter: IntervalFilter,
    ...ops: MatrixToScalar[]
  ): Case | undefined {
    param = map2DArray(param, this.quantize);

    const results = ops.map(o => o(param));
    if (filter === 'finite' && results.some(e => !e.isFinite())) {
      return undefined;
    }

    return {
      input: [toMatrix(param, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateMatrixToScalarCases(
    params: number[][][],
    filter: IntervalFilter,
    ...ops: MatrixToScalar[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeMatrixToScalarCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the param and an array of interval generators provided
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  private makeMatrixToMatrixCase(
    param: number[][],
    filter: IntervalFilter,
    ...ops: MatrixToMatrix[]
  ): Case | undefined {
    param = map2DArray(param, this.quantize);

    const results = ops.map(o => o(param));
    if (filter === 'finite' && results.some(m => m.some(c => c.some(r => !r.isFinite())))) {
      return undefined;
    }

    return {
      input: [toMatrix(param, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  public generateMatrixToMatrixCases(
    params: number[][][],
    filter: IntervalFilter,
    ...ops: MatrixToMatrix[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeMatrixToMatrixCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and matrix of intervals generator provided
   * @param param0 the first param to pass in
   * @param param1 the second param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  private makeMatrixPairToMatrixCase(
    param0: number[][],
    param1: number[][],
    filter: IntervalFilter,
    ...ops: MatrixPairToMatrix[]
  ): Case | undefined {
    param0 = map2DArray(param0, this.quantize);
    param1 = map2DArray(param1, this.quantize);

    const results = ops.map(o => o(param0, param1));
    if (filter === 'finite' && results.some(m => m.some(c => c.some(r => !r.isFinite())))) {
      return undefined;
    }
    return {
      input: [toMatrix(param0, this.scalarBuilder), toMatrix(param1, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param param0s array of inputs to try for the first input
   * @param param1s array of inputs to try for the second input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  public generateMatrixPairToMatrixCases(
    param0s: number[][][],
    param1s: number[][][],
    filter: IntervalFilter,
    ...ops: MatrixPairToMatrix[]
  ): Case[] {
    return cartesianProduct(param0s, param1s).reduce((cases, e) => {
      const c = this.makeMatrixPairToMatrixCase(e[0], e[1], filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  /**
   * @returns a Case for the params and matrix of intervals generator provided
   * @param mat the matrix param to pass in
   * @param scalar the scalar to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  private makeMatrixScalarToMatrixCase(
    mat: number[][],
    scalar: number,
    filter: IntervalFilter,
    ...ops: MatrixScalarToMatrix[]
  ): Case | undefined {
    mat = map2DArray(mat, this.quantize);
    scalar = this.quantize(scalar);

    const results = ops.map(o => o(mat, scalar));
    if (filter === 'finite' && results.some(m => m.some(c => c.some(r => !r.isFinite())))) {
      return undefined;
    }
    return {
      input: [toMatrix(mat, this.scalarBuilder), this.scalarBuilder(scalar)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param mats array of inputs to try for the matrix input
   * @param scalars array of inputs to try for the scalar input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  public generateMatrixScalarToMatrixCases(
    mats: number[][][],
    scalars: number[],
    filter: IntervalFilter,
    ...ops: MatrixScalarToMatrix[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    mats.forEach(mat => {
      scalars.forEach(scalar => {
        const c = this.makeMatrixScalarToMatrixCase(mat, scalar, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  /**
   * @returns a Case for the params and matrix of intervals generator provided
   * @param scalar the scalar to pass in
   * @param mat the matrix param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  private makeScalarMatrixToMatrixCase(
    scalar: number,
    mat: number[][],
    filter: IntervalFilter,
    ...ops: ScalarMatrixToMatrix[]
  ): Case | undefined {
    scalar = this.quantize(scalar);
    mat = map2DArray(mat, this.quantize);

    const results = ops.map(o => o(scalar, mat));
    if (filter === 'finite' && results.some(m => m.some(c => c.some(r => !r.isFinite())))) {
      return undefined;
    }
    return {
      input: [this.scalarBuilder(scalar), toMatrix(mat, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param scalars array of inputs to try for the scalar input
   * @param mats array of inputs to try for the matrix input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a matrix of acceptance
   *            intervals
   */
  public generateScalarMatrixToMatrixCases(
    scalars: number[],
    mats: number[][][],
    filter: IntervalFilter,
    ...ops: ScalarMatrixToMatrix[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    mats.forEach(mat => {
      scalars.forEach(scalar => {
        const c = this.makeScalarMatrixToMatrixCase(scalar, mat, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  /**
   * @returns a Case for the params and the vector of intervals generator provided
   * @param mat the matrix param to pass in
   * @param vec the vector to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals
   */
  private makeMatrixVectorToVectorCase(
    mat: number[][],
    vec: number[],
    filter: IntervalFilter,
    ...ops: MatrixVectorToVector[]
  ): Case | undefined {
    mat = map2DArray(mat, this.quantize);
    vec = vec.map(this.quantize);

    const results = ops.map(o => o(mat, vec));
    if (filter === 'finite' && results.some(v => v.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [toMatrix(mat, this.scalarBuilder), toVector(vec, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param mats array of inputs to try for the matrix input
   * @param vecs array of inputs to try for the vector input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals
   */
  public generateMatrixVectorToVectorCases(
    mats: number[][][],
    vecs: number[][],
    filter: IntervalFilter,
    ...ops: MatrixVectorToVector[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    mats.forEach(mat => {
      vecs.forEach(vec => {
        const c = this.makeMatrixVectorToVectorCase(mat, vec, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  /**
   * @returns a Case for the params and the vector of intervals generator provided
   * @param vec the vector to pass in
   * @param mat the matrix param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals
   */
  private makeVectorMatrixToVectorCase(
    vec: number[],
    mat: number[][],
    filter: IntervalFilter,
    ...ops: VectorMatrixToVector[]
  ): Case | undefined {
    vec = vec.map(this.quantize);
    mat = map2DArray(mat, this.quantize);

    const results = ops.map(o => o(vec, mat));
    if (filter === 'finite' && results.some(v => v.some(e => !e.isFinite()))) {
      return undefined;
    }
    return {
      input: [toVector(vec, this.scalarBuilder), toMatrix(mat, this.scalarBuilder)],
      expected: anyOf(...results),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param vecs array of inputs to try for the vector input
   * @param mats array of inputs to try for the matrix input
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating a vector of acceptance
   *            intervals
   */
  public generateVectorMatrixToVectorCases(
    vecs: number[][],
    mats: number[][][],
    filter: IntervalFilter,
    ...ops: VectorMatrixToVector[]
  ): Case[] {
    // Cannot use cartesianProduct here, due to heterogeneous types
    const cases: Case[] = [];
    vecs.forEach(vec => {
      mats.forEach(mat => {
        const c = this.makeVectorMatrixToVectorCase(vec, mat, filter, ...ops);
        if (c !== undefined) {
          cases.push(c);
        }
      });
    });
    return cases;
  }

  // Framework - Intervals

  /**
   * Converts a point to an acceptance interval, using a specific function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   * op.extrema is invoked before this point in the call stack.
   * op.domain is tested before this point in the call stack.
   *
   * @param n value to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  private roundAndFlushScalarToInterval(n: number, op: ScalarToIntervalOp) {
    assert(!Number.isNaN(n), `flush not defined for NaN`);
    const values = this.correctlyRounded(n);
    const inputs = this.addFlushedIfNeeded(values);
    const results = new Set<FPInterval>(inputs.map(op.impl));
    return this.spanIntervals(...results);
  }

  /**
   * Converts a pair to an acceptance interval, using a specific function
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
   * @returns a span over all the outputs of op.impl
   */
  private roundAndFlushScalarPairToInterval(
    x: number,
    y: number,
    op: ScalarPairToIntervalOp
  ): FPInterval {
    assert(!Number.isNaN(x), `flush not defined for NaN`);
    assert(!Number.isNaN(y), `flush not defined for NaN`);
    const x_values = this.correctlyRounded(x);
    const y_values = this.correctlyRounded(y);
    const x_inputs = this.addFlushedIfNeeded(x_values);
    const y_inputs = this.addFlushedIfNeeded(y_values);
    const intervals = new Set<FPInterval>();
    x_inputs.forEach(inner_x => {
      y_inputs.forEach(inner_y => {
        intervals.add(op.impl(inner_x, inner_y));
      });
    });
    return this.spanIntervals(...intervals);
  }

  /**
   * Converts a triplet to an acceptance interval, using a specific function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   * All unique combinations of x, y & z are run.
   *
   * @param x first param to flush & round then invoke op.impl on
   * @param y second param to flush & round then invoke op.impl on
   * @param z third param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  private roundAndFlushScalarTripleToInterval(
    x: number,
    y: number,
    z: number,
    op: ScalarTripleToIntervalOp
  ): FPInterval {
    assert(!Number.isNaN(x), `flush not defined for NaN`);
    assert(!Number.isNaN(y), `flush not defined for NaN`);
    assert(!Number.isNaN(z), `flush not defined for NaN`);
    const x_values = this.correctlyRounded(x);
    const y_values = this.correctlyRounded(y);
    const z_values = this.correctlyRounded(z);
    const x_inputs = this.addFlushedIfNeeded(x_values);
    const y_inputs = this.addFlushedIfNeeded(y_values);
    const z_inputs = this.addFlushedIfNeeded(z_values);
    const intervals = new Set<FPInterval>();
    // prettier-ignore
    x_inputs.forEach(inner_x => {
      y_inputs.forEach(inner_y => {
        z_inputs.forEach(inner_z => {
          intervals.add(op.impl(inner_x, inner_y, inner_z));
        });
      });
    });

    return this.spanIntervals(...intervals);
  }

  /**
   * Converts a vector to an acceptance interval using a specific function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   *
   * @param x param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  private roundAndFlushVectorToInterval(x: number[], op: VectorToIntervalOp): FPInterval {
    assert(
      x.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );

    const x_rounded: number[][] = x.map(this.correctlyRounded);
    const x_flushed: number[][] = x_rounded.map(this.addFlushedIfNeeded.bind(this));
    const x_inputs = cartesianProduct<number>(...x_flushed);

    const intervals = new Set<FPInterval>();
    x_inputs.forEach(inner_x => {
      intervals.add(op.impl(inner_x));
    });
    return this.spanIntervals(...intervals);
  }

  /**
   * Converts a pair of vectors to an acceptance interval using a specific
   * function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   * All unique combinations of x & y are run.
   *
   * @param x first param to flush & round then invoke op.impl on
   * @param y second param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  private roundAndFlushVectorPairToInterval(
    x: number[],
    y: number[],
    op: VectorPairToIntervalOp
  ): FPInterval {
    assert(
      x.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );
    assert(
      y.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );

    const x_rounded: number[][] = x.map(this.correctlyRounded);
    const y_rounded: number[][] = y.map(this.correctlyRounded);
    const x_flushed: number[][] = x_rounded.map(this.addFlushedIfNeeded.bind(this));
    const y_flushed: number[][] = y_rounded.map(this.addFlushedIfNeeded.bind(this));
    const x_inputs = cartesianProduct<number>(...x_flushed);
    const y_inputs = cartesianProduct<number>(...y_flushed);

    const intervals = new Set<FPInterval>();
    x_inputs.forEach(inner_x => {
      y_inputs.forEach(inner_y => {
        intervals.add(op.impl(inner_x, inner_y));
      });
    });
    return this.spanIntervals(...intervals);
  }

  /**
   * Converts a vector to a vector of acceptance intervals using a specific
   * function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   *
   * @param x param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a vector of spans for each outputs of op.impl
   */
  private roundAndFlushVectorToVector(x: number[], op: VectorToVectorOp): FPVector {
    assert(
      x.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );

    const x_rounded: number[][] = x.map(this.correctlyRounded);
    const x_flushed: number[][] = x_rounded.map(this.addFlushedIfNeeded.bind(this));
    const x_inputs = cartesianProduct<number>(...x_flushed);

    const interval_vectors = new Set<FPVector>();
    x_inputs.forEach(inner_x => {
      interval_vectors.add(op.impl(inner_x));
    });

    return this.spanVectors(...interval_vectors);
  }

  /**
   * Converts a pair of vectors to a vector of acceptance intervals using a
   * specific function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   *
   * @param x first param to flush & round then invoke op.impl on
   * @param y second param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a vector of spans for each output of op.impl
   */
  private roundAndFlushVectorPairToVector(
    x: number[],
    y: number[],
    op: VectorPairToVectorOp
  ): FPVector {
    assert(
      x.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );
    assert(
      y.every(e => !Number.isNaN(e)),
      `flush not defined for NaN`
    );

    const x_rounded: number[][] = x.map(this.correctlyRounded);
    const y_rounded: number[][] = y.map(this.correctlyRounded);
    const x_flushed: number[][] = x_rounded.map(this.addFlushedIfNeeded.bind(this));
    const y_flushed: number[][] = y_rounded.map(this.addFlushedIfNeeded.bind(this));
    const x_inputs = cartesianProduct<number>(...x_flushed);
    const y_inputs = cartesianProduct<number>(...y_flushed);

    const interval_vectors = new Set<FPVector>();
    x_inputs.forEach(inner_x => {
      y_inputs.forEach(inner_y => {
        interval_vectors.add(op.impl(inner_x, inner_y));
      });
    });

    return this.spanVectors(...interval_vectors);
  }

  /**
   * Converts a matrix to a matrix of acceptance intervals using a specific
   * function
   *
   * This handles correctly rounding and flushing inputs as needed.
   * Duplicate inputs are pruned before invoking op.impl.
   *
   * @param m param to flush & round then invoke op.impl on
   * @param op operation defining the function being run
   * @returns a matrix of spans for each outputs of op.impl
   */
  private roundAndFlushMatrixToMatrix(m: Array2D<number>, op: MatrixToMatrixOp): FPMatrix {
    const num_cols = m.length;
    const num_rows = m[0].length;
    assert(
      m.every(c => c.every(r => !Number.isNaN(r))),
      `flush not defined for NaN`
    );

    const m_flat = flatten2DArray(m);
    const m_rounded: number[][] = m_flat.map(this.correctlyRounded);
    const m_flushed: number[][] = m_rounded.map(this.addFlushedIfNeeded.bind(this));
    const m_options: number[][] = cartesianProduct<number>(...m_flushed);
    const m_inputs: Array2D<number>[] = m_options.map(e => unflatten2DArray(e, num_cols, num_rows));

    const interval_matrices = new Set<FPMatrix>();
    m_inputs.forEach(inner_m => {
      interval_matrices.add(op.impl(inner_m));
    });

    return this.spanMatrices(...interval_matrices);
  }

  /**
   * Calculate the acceptance interval for a unary function over an interval
   *
   * If the interval is actually a point, this just decays to
   * roundAndFlushScalarToInterval.
   *
   * The provided domain interval may be adjusted if the operation defines an
   * extrema function.
   *
   * @param x input domain interval
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  protected runScalarToIntervalOp(x: FPInterval, op: ScalarToIntervalOp): FPInterval {
    if (!x.isFinite()) {
      return this.constants().unboundedInterval;
    }

    if (op.extrema !== undefined) {
      x = op.extrema(x);
    }

    const result = this.spanIntervals(
      ...x.bounds().map(b => this.roundAndFlushScalarToInterval(b, op))
    );
    return result.isFinite() ? result : this.constants().unboundedInterval;
  }

  /**
   * Calculate the acceptance interval for a binary function over an interval
   *
   * The provided domain intervals may be adjusted if the operation defines an
   * extrema function.
   *
   * @param x first input domain interval
   * @param y second input domain interval
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  protected runScalarPairToIntervalOp(
    x: FPInterval,
    y: FPInterval,
    op: ScalarPairToIntervalOp
  ): FPInterval {
    if (!x.isFinite() || !y.isFinite()) {
      return this.constants().unboundedInterval;
    }

    if (op.extrema !== undefined) {
      [x, y] = op.extrema(x, y);
    }

    const outputs = new Set<FPInterval>();
    x.bounds().forEach(inner_x => {
      y.bounds().forEach(inner_y => {
        outputs.add(this.roundAndFlushScalarPairToInterval(inner_x, inner_y, op));
      });
    });

    const result = this.spanIntervals(...outputs);
    return result.isFinite() ? result : this.constants().unboundedInterval;
  }

  /**
   * Calculate the acceptance interval for a ternary function over an interval
   *
   * @param x first input domain interval
   * @param y second input domain interval
   * @param z third input domain interval
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  protected runScalarTripleToIntervalOp(
    x: FPInterval,
    y: FPInterval,
    z: FPInterval,
    op: ScalarTripleToIntervalOp
  ): FPInterval {
    if (!x.isFinite() || !y.isFinite() || !z.isFinite()) {
      return this.constants().unboundedInterval;
    }

    const outputs = new Set<FPInterval>();
    x.bounds().forEach(inner_x => {
      y.bounds().forEach(inner_y => {
        z.bounds().forEach(inner_z => {
          outputs.add(this.roundAndFlushScalarTripleToInterval(inner_x, inner_y, inner_z, op));
        });
      });
    });

    const result = this.spanIntervals(...outputs);
    return result.isFinite() ? result : this.constants().unboundedInterval;
  }

  /**
   * Calculate the acceptance interval for a vector function over given
   * intervals
   *
   * @param x input domain intervals vector
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  protected runVectorToIntervalOp(x: FPVector, op: VectorToIntervalOp): FPInterval {
    if (x.some(e => !e.isFinite())) {
      return this.constants().unboundedInterval;
    }

    const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));

    const outputs = new Set<FPInterval>();
    x_values.forEach(inner_x => {
      outputs.add(this.roundAndFlushVectorToInterval(inner_x, op));
    });

    const result = this.spanIntervals(...outputs);
    return result.isFinite() ? result : this.constants().unboundedInterval;
  }

  /**
   * Calculate the acceptance interval for a vector pair function over given
   * intervals
   *
   * @param x first input domain intervals vector
   * @param y second input domain intervals vector
   * @param op operation defining the function being run
   * @returns a span over all the outputs of op.impl
   */
  protected runVectorPairToIntervalOp(
    x: FPVector,
    y: FPVector,
    op: VectorPairToIntervalOp
  ): FPInterval {
    if (x.some(e => !e.isFinite()) || y.some(e => !e.isFinite())) {
      return this.constants().unboundedInterval;
    }

    const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));
    const y_values = cartesianProduct<number>(...y.map(e => e.bounds()));

    const outputs = new Set<FPInterval>();
    x_values.forEach(inner_x => {
      y_values.forEach(inner_y => {
        outputs.add(this.roundAndFlushVectorPairToInterval(inner_x, inner_y, op));
      });
    });

    const result = this.spanIntervals(...outputs);
    return result.isFinite() ? result : this.constants().unboundedInterval;
  }

  /**
   * Calculate the vector of acceptance intervals for a pair of vector function
   * over given intervals
   *
   * @param x input domain intervals vector
   * @param op operation defining the function being run
   * @returns a vector of spans over all the outputs of op.impl
   */
  protected runVectorToVectorOp(x: FPVector, op: VectorToVectorOp): FPVector {
    if (x.some(e => !e.isFinite())) {
      return this.constants().unboundedVector[x.length];
    }

    const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));

    const outputs = new Set<FPVector>();
    x_values.forEach(inner_x => {
      outputs.add(this.roundAndFlushVectorToVector(inner_x, op));
    });

    const result = this.spanVectors(...outputs);
    return result.every(e => e.isFinite())
      ? result
      : this.constants().unboundedVector[result.length];
  }

  /**
   * Calculate the vector of acceptance intervals by running a scalar operation
   * component-wise over a vector.
   *
   * This is used for situations where a component-wise operation, like vector
   * negation, is needed as part of an inherited accuracy, but the top-level
   * operation test don't require an explicit vector definition of the function,
   * due to the generated 'vectorize' tests being sufficient.
   *
   * @param x input domain intervals vector
   * @param op scalar operation to be run component-wise
   * @returns a vector of intervals with the outputs of op.impl
   */
  protected runScalarToIntervalOpComponentWise(x: FPVector, op: ScalarToIntervalOp): FPVector {
    return this.toVector(x.map(e => this.runScalarToIntervalOp(e, op)));
  }

  /**
   * Calculate the vector of acceptance intervals for a vector function over
   * given intervals
   *
   * @param x first input domain intervals vector
   * @param y second input domain intervals vector
   * @param op operation defining the function being run
   * @returns a vector of spans over all the outputs of op.impl
   */
  protected runVectorPairToVectorOp(x: FPVector, y: FPVector, op: VectorPairToVectorOp): FPVector {
    if (x.some(e => !e.isFinite()) || y.some(e => !e.isFinite())) {
      return this.constants().unboundedVector[x.length];
    }

    const x_values = cartesianProduct<number>(...x.map(e => e.bounds()));
    const y_values = cartesianProduct<number>(...y.map(e => e.bounds()));

    const outputs = new Set<FPVector>();
    x_values.forEach(inner_x => {
      y_values.forEach(inner_y => {
        outputs.add(this.roundAndFlushVectorPairToVector(inner_x, inner_y, op));
      });
    });

    const result = this.spanVectors(...outputs);
    return result.every(e => e.isFinite())
      ? result
      : this.constants().unboundedVector[result.length];
  }

  /**
   * Calculate the vector of acceptance intervals by running a scalar operation
   * component-wise over a pair of vectors.
   *
   * This is used for situations where a component-wise operation, like vector
   * subtraction, is needed as part of an inherited accuracy, but the top-level
   * operation test don't require an explicit vector definition of the function,
   * due to the generated 'vectorize' tests being sufficient.
   *
   * @param x first input domain intervals vector
   * @param y second input domain intervals vector
   * @param op scalar operation to be run component-wise
   * @returns a vector of intervals with the outputs of op.impl
   */
  protected runScalarPairToIntervalOpVectorComponentWise(
    x: FPVector,
    y: FPVector,
    op: ScalarPairToIntervalOp
  ): FPVector {
    assert(
      x.length === y.length,
      `runScalarPairToIntervalOpVectorComponentWise requires vectors of the same dimensions`
    );

    return this.toVector(
      x.map((i, idx) => {
        return this.runScalarPairToIntervalOp(i, y[idx], op);
      })
    );
  }

  /**
   * Calculate the matrix of acceptance intervals for a pair of matrix function over
   * given intervals
   *
   * @param m input domain intervals matrix
   * @param op operation defining the function being run
   * @returns a matrix of spans over all the outputs of op.impl
   */
  protected runMatrixToMatrixOp(m: FPMatrix, op: MatrixToMatrixOp): FPMatrix {
    const num_cols = m.length;
    const num_rows = m[0].length;
    if (m.some(c => c.some(r => !r.isFinite()))) {
      return this.constants().unboundedMatrix[num_cols][num_rows];
    }

    const m_flat: FPInterval[] = flatten2DArray(m);
    const m_values: number[][] = cartesianProduct<number>(...m_flat.map(e => e.bounds()));

    const outputs = new Set<FPMatrix>();
    m_values.forEach(inner_m => {
      const unflat_m = unflatten2DArray(inner_m, num_cols, num_rows);
      outputs.add(this.roundAndFlushMatrixToMatrix(unflat_m, op));
    });

    const result = this.spanMatrices(...outputs);
    const result_cols = result.length;
    const result_rows = result[0].length;

    // FPMatrix has to be coerced to FPInterval[][] to use .every. This should
    // always be safe, since FPMatrix are defined as fixed length array of
    // arrays.
    return (result as FPInterval[][]).every(c => c.every(r => r.isFinite()))
      ? result
      : this.constants().unboundedMatrix[result_cols][result_rows];
  }

  /**
   * Calculate the Matrix of acceptance intervals by running a scalar operation
   * component-wise over a pair of matrices.
   *
   * An example of this is performing matrix addition.
   *
   * @param x first input domain intervals matrix
   * @param y second input domain intervals matrix
   * @param op scalar operation to be run component-wise
   * @returns a matrix of intervals with the outputs of op.impl
   */
  protected runScalarPairToIntervalOpMatrixComponentWise(
    x: FPMatrix,
    y: FPMatrix,
    op: ScalarPairToIntervalOp
  ): FPMatrix {
    assert(
      x.length === y.length && x[0].length === y[0].length,
      `runScalarPairToIntervalOpMatrixComponentWise requires matrices of the same dimensions`
    );

    const cols = x.length;
    const rows = x[0].length;
    const flat_x = flatten2DArray(x);
    const flat_y = flatten2DArray(y);

    return this.toMatrix(
      unflatten2DArray(
        flat_x.map((i, idx) => {
          return this.runScalarPairToIntervalOp(i, flat_y[idx], op);
        }),
        cols,
        rows
      )
    );
  }

  // API - Fundamental Error Intervals

  /** @returns a ScalarToIntervalOp for [n - error_range, n + error_range] */
  private AbsoluteErrorIntervalOp(error_range: number): ScalarToIntervalOp {
    const op: ScalarToIntervalOp = {
      impl: (_: number) => {
        return this.constants().unboundedInterval;
      },
    };

    assert(
      error_range >= 0,
      `absoluteErrorInterval must have non-negative error range, get ${error_range}`
    );

    if (this.isFinite(error_range)) {
      op.impl = (n: number) => {
        assert(!Number.isNaN(n), `absolute error not defined for NaN`);
        // Return anyInterval if given center n is infinity.
        if (!this.isFinite(n)) {
          return this.constants().unboundedInterval;
        }
        return this.toInterval([n - error_range, n + error_range]);
      };
    }

    return op;
  }

  protected absoluteErrorIntervalImpl(n: number, error_range: number): FPInterval {
    error_range = Math.abs(error_range);
    return this.runScalarToIntervalOp(
      this.toInterval(n),
      this.AbsoluteErrorIntervalOp(error_range)
    );
  }

  /** @returns an interval of the absolute error around the point */
  public abstract readonly absoluteErrorInterval: (n: number, error_range: number) => FPInterval;

  /**
   * Defines a ScalarToIntervalOp for an interval of the correctly rounded values
   * around the point
   */
  private readonly CorrectlyRoundedIntervalOp: ScalarToIntervalOp = {
    impl: (n: number) => {
      assert(!Number.isNaN(n), `absolute not defined for NaN`);
      return this.toInterval(n);
    },
  };

  protected correctlyRoundedIntervalImpl(n: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.CorrectlyRoundedIntervalOp);
  }

  /** @returns an interval of the correctly rounded values around the point */
  public abstract readonly correctlyRoundedInterval: (n: number | FPInterval) => FPInterval;

  protected correctlyRoundedMatrixImpl(m: Array2D<number>): FPMatrix {
    return this.toMatrix(map2DArray(m, this.correctlyRoundedInterval));
  }

  /** @returns a matrix of correctly rounded intervals for the provided matrix */
  public abstract readonly correctlyRoundedMatrix: (m: Array2D<number>) => FPMatrix;

  /** @returns a ScalarToIntervalOp for [n - numULP * ULP(n), n + numULP * ULP(n)] */
  private ULPIntervalOp(numULP: number): ScalarToIntervalOp {
    const op: ScalarToIntervalOp = {
      impl: (_: number) => {
        return this.constants().unboundedInterval;
      },
    };

    if (this.isFinite(numULP)) {
      op.impl = (n: number) => {
        assert(!Number.isNaN(n), `ULP error not defined for NaN`);

        const ulp = this.oneULP(n);
        const begin = n - numULP * ulp;
        const end = n + numULP * ulp;

        return this.toInterval([
          Math.min(begin, this.flushSubnormal(begin)),
          Math.max(end, this.flushSubnormal(end)),
        ]);
      };
    }

    return op;
  }

  protected ulpIntervalImpl(n: number, numULP: number): FPInterval {
    numULP = Math.abs(numULP);
    return this.runScalarToIntervalOp(this.toInterval(n), this.ULPIntervalOp(numULP));
  }

  /** @returns an interval of N * ULP around the point */
  public abstract readonly ulpInterval: (n: number, numULP: number) => FPInterval;

  // API - Acceptance Intervals

  private readonly AbsIntervalOp: ScalarToIntervalOp = {
    impl: (n: number) => {
      return this.correctlyRoundedInterval(Math.abs(n));
    },
  };

  protected absIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AbsIntervalOp);
  }

  /** Calculate an acceptance interval for abs(n) */
  public abstract readonly absInterval: (n: number) => FPInterval;

  // This op is implemented differently for f32 and f16.
  private readonly AcosIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(this.toInterval([-1.0, 1.0]), (n: number) => {
      assert(this.kind === 'f32' || this.kind === 'f16');
      // acos(n) = atan2(sqrt(1.0 - n * n), n) or a polynomial approximation with absolute error
      const y = this.sqrtInterval(this.subtractionInterval(1, this.multiplicationInterval(n, n)));
      const approx_abs_error = this.kind === 'f32' ? 6.77e-5 : 3.91e-3;
      return this.spanIntervals(
        this.atan2Interval(y, n),
        this.absoluteErrorInterval(Math.acos(n), approx_abs_error)
      );
    }),
  };

  protected acosIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AcosIntervalOp);
  }

  /** Calculate an acceptance interval for acos(n) */
  public abstract readonly acosInterval: (n: number) => FPInterval;

  private readonly AcoshAlternativeIntervalOp: ScalarToIntervalOp = {
    impl: (x: number): FPInterval => {
      // acosh(x) = log(x + sqrt((x + 1.0f) * (x - 1.0)))
      const inner_value = this.multiplicationInterval(
        this.additionInterval(x, 1.0),
        this.subtractionInterval(x, 1.0)
      );
      const sqrt_value = this.sqrtInterval(inner_value);
      return this.logInterval(this.additionInterval(x, sqrt_value));
    },
  };

  protected acoshAlternativeIntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.AcoshAlternativeIntervalOp);
  }

  /** Calculate an acceptance interval of acosh(x) using log(x + sqrt((x + 1.0f) * (x - 1.0))) */
  public abstract readonly acoshAlternativeInterval: (x: number | FPInterval) => FPInterval;

  private readonly AcoshPrimaryIntervalOp: ScalarToIntervalOp = {
    impl: (x: number): FPInterval => {
      // acosh(x) = log(x + sqrt(x * x - 1.0))
      const inner_value = this.subtractionInterval(this.multiplicationInterval(x, x), 1.0);
      const sqrt_value = this.sqrtInterval(inner_value);
      return this.logInterval(this.additionInterval(x, sqrt_value));
    },
  };

  protected acoshPrimaryIntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.AcoshPrimaryIntervalOp);
  }

  /** Calculate an acceptance interval of acosh(x) using log(x + sqrt(x * x - 1.0)) */
  protected abstract acoshPrimaryInterval: (x: number | FPInterval) => FPInterval;

  /** All acceptance interval functions for acosh(x) */
  public abstract readonly acoshIntervals: ScalarToInterval[];

  private readonly AdditionIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      return this.correctlyRoundedInterval(x + y);
    },
  };

  protected additionIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.AdditionIntervalOp
    );
  }

  /** Calculate an acceptance interval of x + y, when x and y are both scalars */
  public abstract readonly additionInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  protected additionMatrixMatrixIntervalImpl(x: Array2D<number>, y: Array2D<number>): FPMatrix {
    return this.runScalarPairToIntervalOpMatrixComponentWise(
      this.toMatrix(x),
      this.toMatrix(y),
      this.AdditionIntervalOp
    );
  }

  /** Calculate an acceptance interval of x + y, when x and y are matrices */
  public abstract readonly additionMatrixMatrixInterval: (
    x: Array2D<number>,
    y: Array2D<number>
  ) => FPMatrix;

  // This op is implemented differently for f32 and f16.
  private readonly AsinIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(this.toInterval([-1.0, 1.0]), (n: number) => {
      assert(this.kind === 'f32' || this.kind === 'f16');
      // asin(n) = atan2(n, sqrt(1.0 - n * n)) or a polynomial approximation with absolute error
      const x = this.sqrtInterval(this.subtractionInterval(1, this.multiplicationInterval(n, n)));
      const approx_abs_error = this.kind === 'f32' ? 6.77e-5 : 3.91e-3;
      return this.spanIntervals(
        this.atan2Interval(n, x),
        this.absoluteErrorInterval(Math.asin(n), approx_abs_error)
      );
    }),
  };

  /** Calculate an acceptance interval for asin(n) */
  protected asinIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AsinIntervalOp);
  }

  /** Calculate an acceptance interval for asin(n) */
  public abstract readonly asinInterval: (n: number) => FPInterval;

  private readonly AsinhIntervalOp: ScalarToIntervalOp = {
    impl: (x: number): FPInterval => {
      // asinh(x) = log(x + sqrt(x * x + 1.0))
      const inner_value = this.additionInterval(this.multiplicationInterval(x, x), 1.0);
      const sqrt_value = this.sqrtInterval(inner_value);
      return this.logInterval(this.additionInterval(x, sqrt_value));
    },
  };

  protected asinhIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AsinhIntervalOp);
  }

  /** Calculate an acceptance interval of asinh(x) */
  public abstract readonly asinhInterval: (n: number) => FPInterval;

  private readonly AtanIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      assert(this.kind === 'f32' || this.kind === 'f16');
      const ulp_error = this.kind === 'f32' ? 4096 : 5;
      return this.ulpInterval(Math.atan(n), ulp_error);
    },
  };

  /** Calculate an acceptance interval of atan(x) */
  protected atanIntervalImpl(n: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AtanIntervalOp);
  }

  /** Calculate an acceptance interval of atan(x) */
  public abstract readonly atanInterval: (n: number | FPInterval) => FPInterval;

  // This op is implemented differently for f32 and f16.
  private Atan2IntervalOpBuilder(): ScalarPairToIntervalOp {
    assert(this.kind === 'f32' || this.kind === 'f16');
    const constants = this.constants();
    // For atan2, the params are labelled (y, x), not (x, y), so domain.x is first parameter (y),
    // and domain.y is the second parameter (x).
    // The first param must be finite and normal.
    const domain_x = [
      this.toInterval([constants.negative.min, constants.negative.max]),
      this.toInterval([constants.positive.min, constants.positive.max]),
    ];
    // inherited from division
    const domain_y =
      this.kind === 'f32'
        ? [this.toInterval([-(2 ** 126), -(2 ** -126)]), this.toInterval([2 ** -126, 2 ** 126])]
        : [this.toInterval([-(2 ** 14), -(2 ** -14)]), this.toInterval([2 ** -14, 2 ** 14])];
    const ulp_error = this.kind === 'f32' ? 4096 : 5;
    return {
      impl: this.limitScalarPairToIntervalDomain(
        {
          x: domain_x,
          y: domain_y,
        },
        (y: number, x: number): FPInterval => {
          // Accurate result in f64
          let atan_yx = Math.atan(y / x);
          // Offset by +/-pi according to the definition. Use pi value in f64 because we are
          // handling accurate result.
          if (x < 0) {
            // x < 0, y > 0, result is atan(y/x) + π
            if (y > 0) {
              atan_yx = atan_yx + kValue.f64.positive.pi.whole;
            } else {
              // x < 0, y < 0, result is atan(y/x) - π
              atan_yx = atan_yx - kValue.f64.positive.pi.whole;
            }
          }

          return this.ulpInterval(atan_yx, ulp_error);
        }
      ),
      extrema: (y: FPInterval, x: FPInterval): [FPInterval, FPInterval] => {
        // There is discontinuity, which generates an unbounded result, at y/x = 0 that will dominate the accuracy
        if (y.contains(0)) {
          if (x.contains(0)) {
            return [this.toInterval(0), this.toInterval(0)];
          }
          return [this.toInterval(0), x];
        }
        return [y, x];
      },
    };
  }

  protected atan2IntervalImpl(y: number | FPInterval, x: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(y),
      this.toInterval(x),
      this.Atan2IntervalOpBuilder()
    );
  }

  /** Calculate an acceptance interval of atan2(y, x) */
  public abstract readonly atan2Interval: (
    y: number | FPInterval,
    x: number | FPInterval
  ) => FPInterval;

  private readonly AtanhIntervalOp: ScalarToIntervalOp = {
    impl: (n: number) => {
      // atanh(x) = log((1.0 + x) / (1.0 - x)) * 0.5
      const numerator = this.additionInterval(1.0, n);
      const denominator = this.subtractionInterval(1.0, n);
      const log_interval = this.logInterval(this.divisionInterval(numerator, denominator));
      return this.multiplicationInterval(log_interval, 0.5);
    },
  };

  protected atanhIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.AtanhIntervalOp);
  }

  /** Calculate an acceptance interval of atanh(x) */
  public abstract readonly atanhInterval: (n: number) => FPInterval;

  private readonly CeilIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.correctlyRoundedInterval(Math.ceil(n));
    },
  };

  protected ceilIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.CeilIntervalOp);
  }

  /** Calculate an acceptance interval of ceil(x) */
  public abstract readonly ceilInterval: (n: number) => FPInterval;

  private readonly ClampMedianIntervalOp: ScalarTripleToIntervalOp = {
    impl: (x: number, y: number, z: number): FPInterval => {
      return this.correctlyRoundedInterval(
        // Default sort is string sort, so have to implement numeric comparison.
        // Cannot use the b-a one-liner, because that assumes no infinities.
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

  protected clampMedianIntervalImpl(
    x: number | FPInterval,
    y: number | FPInterval,
    z: number | FPInterval
  ): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.toInterval(z),
      this.ClampMedianIntervalOp
    );
  }

  /** Calculate an acceptance interval of clamp(x, y, z) via median(x, y, z) */
  public abstract readonly clampMedianInterval: (
    x: number | FPInterval,
    y: number | FPInterval,
    z: number | FPInterval
  ) => FPInterval;

  private readonly ClampMinMaxIntervalOp: ScalarTripleToIntervalOp = {
    impl: (x: number, low: number, high: number): FPInterval => {
      return this.minInterval(this.maxInterval(x, low), high);
    },
  };

  protected clampMinMaxIntervalImpl(
    x: number | FPInterval,
    low: number | FPInterval,
    high: number | FPInterval
  ): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(x),
      this.toInterval(low),
      this.toInterval(high),
      this.ClampMinMaxIntervalOp
    );
  }

  /** Calculate an acceptance interval of clamp(x, high, low) via min(max(x, low), high) */
  public abstract readonly clampMinMaxInterval: (
    x: number | FPInterval,
    low: number | FPInterval,
    high: number | FPInterval
  ) => FPInterval;

  /** All acceptance interval functions for clamp(x, y, z) */
  public abstract readonly clampIntervals: ScalarTripleToInterval[];

  private readonly CosIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(
      this.constants().negPiToPiInterval,
      (n: number): FPInterval => {
        assert(this.kind === 'f32' || this.kind === 'f16');
        const abs_error = this.kind === 'f32' ? 2 ** -11 : 2 ** -7;
        return this.absoluteErrorInterval(Math.cos(n), abs_error);
      }
    ),
  };

  protected cosIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.CosIntervalOp);
  }

  /** Calculate an acceptance interval of cos(x) */
  public abstract readonly cosInterval: (n: number) => FPInterval;

  private readonly CoshIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      // cosh(x) = (exp(x) + exp(-x)) * 0.5
      const minus_n = this.negationInterval(n);
      return this.multiplicationInterval(
        this.additionInterval(this.expInterval(n), this.expInterval(minus_n)),
        0.5
      );
    },
  };

  protected coshIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.CoshIntervalOp);
  }

  /** Calculate an acceptance interval of cosh(x) */
  public abstract readonly coshInterval: (n: number) => FPInterval;

  private readonly CrossIntervalOp: VectorPairToVectorOp = {
    impl: (x: number[], y: number[]): FPVector => {
      assert(x.length === 3, `CrossIntervalOp received x with ${x.length} instead of 3`);
      assert(y.length === 3, `CrossIntervalOp received y with ${y.length} instead of 3`);

      // cross(x, y) = r, where
      //   r[0] = x[1] * y[2] - x[2] * y[1]
      //   r[1] = x[2] * y[0] - x[0] * y[2]
      //   r[2] = x[0] * y[1] - x[1] * y[0]

      const r0 = this.subtractionInterval(
        this.multiplicationInterval(x[1], y[2]),
        this.multiplicationInterval(x[2], y[1])
      );
      const r1 = this.subtractionInterval(
        this.multiplicationInterval(x[2], y[0]),
        this.multiplicationInterval(x[0], y[2])
      );
      const r2 = this.subtractionInterval(
        this.multiplicationInterval(x[0], y[1]),
        this.multiplicationInterval(x[1], y[0])
      );
      return [r0, r1, r2];
    },
  };

  protected crossIntervalImpl(x: number[], y: number[]): FPVector {
    assert(x.length === 3, `Cross is only defined for vec3`);
    assert(y.length === 3, `Cross is only defined for vec3`);
    return this.runVectorPairToVectorOp(this.toVector(x), this.toVector(y), this.CrossIntervalOp);
  }

  /** Calculate a vector of acceptance intervals for cross(x, y) */
  public abstract readonly crossInterval: (x: number[], y: number[]) => FPVector;

  private readonly DegreesIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.multiplicationInterval(n, 57.295779513082322865);
    },
  };

  protected degreesIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.DegreesIntervalOp);
  }

  /** Calculate an acceptance interval of degrees(x) */
  public abstract readonly degreesInterval: (n: number) => FPInterval;

  /**
   * Calculate the minor of a NxN matrix.
   *
   * The ijth minor of a square matrix, is the N-1xN-1 matrix created by removing
   * the ith column and jth row from the original matrix.
   */
  private minorNxN(m: Array2D<number>, col: number, row: number): Array2D<number> {
    const dim = m.length;
    assert(m.length === m[0].length, `minorMatrix is only defined for square matrices`);
    assert(col >= 0 && col < dim, `col ${col} needs be in [0, # of columns '${dim}')`);
    assert(row >= 0 && row < dim, `row ${row} needs be in [0, # of rows '${dim}')`);

    const result: Array2D<number> = [...Array(dim - 1)].map(_ => [...Array(dim - 1)]);

    const col_indices: number[] = [...Array(dim).keys()].filter(e => e !== col);
    const row_indices: number[] = [...Array(dim).keys()].filter(e => e !== row);

    col_indices.forEach((c, i) => {
      row_indices.forEach((r, j) => {
        result[i][j] = m[c][r];
      });
    });
    return result;
  }

  /** Calculate an acceptance interval for determinant(m), where m is a 2x2 matrix */
  private determinant2x2Interval(m: Array2D<number>): FPInterval {
    assert(
      m.length === m[0].length && m.length === 2,
      `determinant2x2Interval called on non-2x2 matrix`
    );
    return this.subtractionInterval(
      this.multiplicationInterval(m[0][0], m[1][1]),
      this.multiplicationInterval(m[0][1], m[1][0])
    );
  }

  /** Calculate an acceptance interval for determinant(m), where m is a 3x3 matrix */
  private determinant3x3Interval(m: Array2D<number>): FPInterval {
    assert(
      m.length === m[0].length && m.length === 3,
      `determinant3x3Interval called on non-3x3 matrix`
    );

    // M is a 3x3 matrix
    // det(M) is A + B + C, where A, B, C are three elements in a row/column times
    // their own co-factor.
    // (The co-factor is the determinant of the minor of that position with the
    // appropriate +/-)
    // For simplicity sake A, B, C are calculated as the elements of the first
    // column
    const A = this.multiplicationInterval(
      m[0][0],
      this.determinant2x2Interval(this.minorNxN(m, 0, 0))
    );
    const B = this.multiplicationInterval(
      -m[0][1],
      this.determinant2x2Interval(this.minorNxN(m, 0, 1))
    );
    const C = this.multiplicationInterval(
      m[0][2],
      this.determinant2x2Interval(this.minorNxN(m, 0, 2))
    );

    // Need to calculate permutations, since for fp addition is not associative,
    // so A + B + C is not guaranteed to equal B + C + A, etc.
    const permutations: FPInterval[][] = calculatePermutations([A, B, C]);
    return this.spanIntervals(
      ...permutations.map(p =>
        p.reduce((prev: FPInterval, cur: FPInterval) => this.additionInterval(prev, cur))
      )
    );
  }

  /** Calculate an acceptance interval for determinant(m), where m is a 4x4 matrix */
  private determinant4x4Interval(m: Array2D<number>): FPInterval {
    assert(
      m.length === m[0].length && m.length === 4,
      `determinant3x3Interval called on non-4x4 matrix`
    );

    // M is a 4x4 matrix
    // det(M) is A + B + C + D, where A, B, C, D are four elements in a row/column
    // times their own co-factor.
    // (The co-factor is the determinant of the minor of that position with the
    // appropriate +/-)
    // For simplicity sake A, B, C, D are calculated as the elements of the
    // first column
    const A = this.multiplicationInterval(
      m[0][0],
      this.determinant3x3Interval(this.minorNxN(m, 0, 0))
    );
    const B = this.multiplicationInterval(
      -m[0][1],
      this.determinant3x3Interval(this.minorNxN(m, 0, 1))
    );
    const C = this.multiplicationInterval(
      m[0][2],
      this.determinant3x3Interval(this.minorNxN(m, 0, 2))
    );
    const D = this.multiplicationInterval(
      -m[0][3],
      this.determinant3x3Interval(this.minorNxN(m, 0, 3))
    );

    // Need to calculate permutations, since for fp addition is not associative
    // so A + B + C + D is not guaranteed to equal B + C + A + D, etc.
    const permutations: FPInterval[][] = calculatePermutations([A, B, C, D]);
    return this.spanIntervals(
      ...permutations.map(p =>
        p.reduce((prev: FPInterval, cur: FPInterval) => this.additionInterval(prev, cur))
      )
    );
  }

  /**
   * This code calculates 3x3 and 4x4 determinants using the textbook co-factor
   * method, using the first column for the co-factor selection.
   *
   * For matrices composed of integer elements, e, with |e|^4 < 2**21, this
   * should be fine.
   *
   * For e, where e is subnormal or 4*(e^4) might not be precisely expressible as
   * a f32 values, this approach breaks down, because the rule of all co-factor
   * definitions of determinant being equal doesn't hold in these cases.
   *
   * The general solution for this is to calculate all the permutations of the
   * operations in the worked out formula for determinant.
   * For 3x3 this is tractable, but for 4x4 this works out to ~23! permutations
   * that need to be calculated.
   * Thus, CTS testing and the spec definition of accuracy is restricted to the
   * space that the simple implementation is valid.
   */
  protected determinantIntervalImpl(x: Array2D<number>): FPInterval {
    const dim = x.length;
    assert(
      x[0].length === dim && (dim === 2 || dim === 3 || dim === 4),
      `determinantInterval only defined for 2x2, 3x3 and 4x4 matrices`
    );
    switch (dim) {
      case 2:
        return this.determinant2x2Interval(x);
      case 3:
        return this.determinant3x3Interval(x);
      case 4:
        return this.determinant4x4Interval(x);
    }
    unreachable(
      "determinantInterval called on x, where which has an unexpected dimension of '${dim}'"
    );
  }

  /** Calculate an acceptance interval for determinant(x) */
  public abstract readonly determinantInterval: (x: Array2D<number>) => FPInterval;

  private readonly DistanceIntervalScalarOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      return this.lengthInterval(this.subtractionInterval(x, y));
    },
  };

  private readonly DistanceIntervalVectorOp: VectorPairToIntervalOp = {
    impl: (x: number[], y: number[]): FPInterval => {
      return this.lengthInterval(
        this.runScalarPairToIntervalOpVectorComponentWise(
          this.toVector(x),
          this.toVector(y),
          this.SubtractionIntervalOp
        )
      );
    },
  };

  protected distanceIntervalImpl(x: number | number[], y: number | number[]): FPInterval {
    if (x instanceof Array && y instanceof Array) {
      assert(
        x.length === y.length,
        `distanceInterval requires both params to have the same number of elements`
      );
      return this.runVectorPairToIntervalOp(
        this.toVector(x),
        this.toVector(y),
        this.DistanceIntervalVectorOp
      );
    } else if (!(x instanceof Array) && !(y instanceof Array)) {
      return this.runScalarPairToIntervalOp(
        this.toInterval(x),
        this.toInterval(y),
        this.DistanceIntervalScalarOp
      );
    }
    unreachable(
      `distanceInterval requires both params to both the same type, either scalars or vectors`
    );
  }

  /** Calculate an acceptance interval of distance(x, y) */
  public abstract readonly distanceInterval: (
    x: number | number[],
    y: number | number[]
  ) => FPInterval;

  // This op is implemented differently for f32 and f16.
  private DivisionIntervalOpBuilder(): ScalarPairToIntervalOp {
    assert(this.kind === 'f32' || this.kind === 'f16');
    const constants = this.constants();
    const domain_x = [this.toInterval([constants.negative.min, constants.positive.max])];
    const domain_y =
      this.kind === 'f32'
        ? [this.toInterval([-(2 ** 126), -(2 ** -126)]), this.toInterval([2 ** -126, 2 ** 126])]
        : [this.toInterval([-(2 ** 14), -(2 ** -14)]), this.toInterval([2 ** -14, 2 ** 14])];
    return {
      impl: this.limitScalarPairToIntervalDomain(
        {
          x: domain_x,
          y: domain_y,
        },
        (x: number, y: number): FPInterval => {
          if (y === 0) {
            return constants.unboundedInterval;
          }
          return this.ulpInterval(x / y, 2.5);
        }
      ),
      extrema: (x: FPInterval, y: FPInterval): [FPInterval, FPInterval] => {
        // division has a discontinuity at y = 0.
        if (y.contains(0)) {
          y = this.toInterval(0);
        }
        return [x, y];
      },
    };
  }

  protected divisionIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    assert(this.kind === 'f32' || this.kind === 'f16');
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.DivisionIntervalOpBuilder()
    );
  }

  /** Calculate an acceptance interval of x / y */
  public abstract readonly divisionInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  private readonly DotIntervalOp: VectorPairToIntervalOp = {
    impl: (x: number[], y: number[]): FPInterval => {
      // dot(x, y) = sum of x[i] * y[i]
      const multiplications = this.runScalarPairToIntervalOpVectorComponentWise(
        this.toVector(x),
        this.toVector(y),
        this.MultiplicationIntervalOp
      );

      // vec2 doesn't require permutations, since a + b = b + a for floats
      if (multiplications.length === 2) {
        return this.additionInterval(multiplications[0], multiplications[1]);
      }

      // The spec does not state the ordering of summation, so all the
      // permutations are calculated and their results spanned, since addition
      // of more than two floats is not transitive, i.e. a + b + c is not
      // guaranteed to equal b + a + c
      const permutations: FPInterval[][] = calculatePermutations(multiplications);
      return this.spanIntervals(
        ...permutations.map(p => p.reduce((prev, cur) => this.additionInterval(prev, cur)))
      );
    },
  };

  protected dotIntervalImpl(x: number[] | FPInterval[], y: number[] | FPInterval[]): FPInterval {
    assert(x.length === y.length, `dot not defined for vectors with different lengths`);
    return this.runVectorPairToIntervalOp(this.toVector(x), this.toVector(y), this.DotIntervalOp);
  }

  /** Calculated the acceptance interval for dot(x, y) */
  public abstract readonly dotInterval: (
    x: number[] | FPInterval[],
    y: number[] | FPInterval[]
  ) => FPInterval;

  private readonly ExpIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.ulpInterval(Math.exp(n), 3 + 2 * Math.abs(n));
    },
  };

  protected expIntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.ExpIntervalOp);
  }

  /** Calculate an acceptance interval for exp(x) */
  public abstract readonly expInterval: (x: number | FPInterval) => FPInterval;

  private readonly Exp2IntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.ulpInterval(Math.pow(2, n), 3 + 2 * Math.abs(n));
    },
  };

  protected exp2IntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.Exp2IntervalOp);
  }

  /** Calculate an acceptance interval for exp2(x) */
  public abstract readonly exp2Interval: (x: number | FPInterval) => FPInterval;

  /**
   * faceForward(x, y, z) = select(-x, x, dot(z, y) < 0.0)
   *
   * This builtin selects from two discrete results (delta rounding/flushing),
   * so the majority of the framework code is not appropriate, since the
   * framework attempts to span results.
   *
   * Thus, a bespoke implementation is used instead of
   * defining an Op and running that through the framework.
   */
  protected faceForwardIntervalsImpl(
    x: number[],
    y: number[],
    z: number[]
  ): (FPVector | undefined)[] {
    const x_vec = this.toVector(x);
    // Running vector through this.runScalarToIntervalOpComponentWise to make
    // sure that flushing/rounding is handled, since toVector does not perform
    // those operations.
    const positive_x = this.runScalarToIntervalOpComponentWise(x_vec, {
      impl: (i: number): FPInterval => {
        return this.toInterval(i);
      },
    });
    const negative_x = this.runScalarToIntervalOpComponentWise(x_vec, this.NegationIntervalOp);

    const dot_interval = this.dotInterval(z, y);

    const results: (FPVector | undefined)[] = [];

    if (!dot_interval.isFinite()) {
      // dot calculation went out of bounds
      // Inserting undefined in the result, so that the test running framework
      // is aware of this potential OOB.
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

  /** Calculate the acceptance intervals for faceForward(x, y, z) */
  public abstract readonly faceForwardIntervals: (
    x: number[],
    y: number[],
    z: number[]
  ) => (FPVector | undefined)[];

  private readonly FloorIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.correctlyRoundedInterval(Math.floor(n));
    },
  };

  protected floorIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.FloorIntervalOp);
  }

  /** Calculate an acceptance interval of floor(x) */
  public abstract readonly floorInterval: (n: number) => FPInterval;

  private readonly FmaIntervalOp: ScalarTripleToIntervalOp = {
    impl: (x: number, y: number, z: number): FPInterval => {
      return this.additionInterval(this.multiplicationInterval(x, y), z);
    },
  };

  protected fmaIntervalImpl(x: number, y: number, z: number): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.toInterval(z),
      this.FmaIntervalOp
    );
  }

  /** Calculate an acceptance interval for fma(x, y, z) */
  public abstract readonly fmaInterval: (x: number, y: number, z: number) => FPInterval;

  private readonly FractIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      // fract(x) = x - floor(x) is defined in the spec.
      // For people coming from a non-graphics background this will cause some
      // unintuitive results. For example,
      // fract(-1.1) is not 0.1 or -0.1, but instead 0.9.
      // This is how other shading languages operate and allows for a desirable
      // wrap around in graphics programming.
      const result = this.subtractionInterval(n, this.floorInterval(n));
      assert(
        // negative.subnormal.min instead of 0, because FTZ can occur
        // selectively during the calculation
        this.toInterval([this.constants().negative.subnormal.min, 1.0]).contains(result),
        `fract(${n}) interval [${result}] unexpectedly extends beyond [~0.0, 1.0]`
      );
      if (result.contains(1)) {
        // Very small negative numbers can lead to catastrophic cancellation,
        // thus calculating a fract of 1.0, which is technically not a
        // fractional part, so some implementations clamp the result to next
        // nearest number.
        return this.spanIntervals(result, this.toInterval(this.constants().positive.less_than_one));
      }
      return result;
    },
  };

  protected fractIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.FractIntervalOp);
  }

  /** Calculate an acceptance interval of fract(x) */
  public abstract readonly fractInterval: (n: number) => FPInterval;

  private readonly InverseSqrtIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(
      this.constants().greaterThanZeroInterval,
      (n: number): FPInterval => {
        return this.ulpInterval(1 / Math.sqrt(n), 2);
      }
    ),
  };

  protected inverseSqrtIntervalImpl(n: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.InverseSqrtIntervalOp);
  }

  /** Calculate an acceptance interval of inverseSqrt(x) */
  public abstract readonly inverseSqrtInterval: (n: number | FPInterval) => FPInterval;

  // This op should be implemented differently for f32 and f16.
  private readonly LdexpIntervalOp: ScalarPairToIntervalOp = {
    impl: this.limitScalarPairToIntervalDomain(
      // Implementing SPIR-V's more restrictive domain until
      // https://github.com/gpuweb/gpuweb/issues/3134 is resolved
      {
        x: [this.toInterval([kValue.f32.negative.min, kValue.f32.positive.max])],
        y: [this.toInterval([-126, 128])],
      },
      (e1: number, e2: number): FPInterval => {
        // Though the spec says the result of ldexp(e1, e2) = e1 * 2 ^ e2, the
        // accuracy is listed as correctly rounded to the true value, so the
        // inheritance framework does not need to be invoked to determine
        // bounds.
        // Instead, the value at a higher precision is calculated and passed to
        // correctlyRoundedInterval.
        const result = e1 * 2 ** e2;
        if (Number.isNaN(result)) {
          // Overflowed TS's number type, so definitely out of bounds for f32
          return this.constants().unboundedInterval;
        }
        return this.correctlyRoundedInterval(result);
      }
    ),
  };

  protected ldexpIntervalImpl(e1: number, e2: number): FPInterval {
    return this.roundAndFlushScalarPairToInterval(e1, e2, this.LdexpIntervalOp);
  }

  /** Calculate an acceptance interval of ldexp(e1, e2) */
  public abstract readonly ldexpInterval: (e1: number, e2: number) => FPInterval;

  private readonly LengthIntervalScalarOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.sqrtInterval(this.multiplicationInterval(n, n));
    },
  };

  private readonly LengthIntervalVectorOp: VectorToIntervalOp = {
    impl: (n: number[]): FPInterval => {
      return this.sqrtInterval(this.dotInterval(n, n));
    },
  };

  protected lengthIntervalImpl(n: number | FPInterval | number[] | FPVector): FPInterval {
    if (n instanceof Array) {
      return this.runVectorToIntervalOp(this.toVector(n), this.LengthIntervalVectorOp);
    } else {
      return this.runScalarToIntervalOp(this.toInterval(n), this.LengthIntervalScalarOp);
    }
  }

  /** Calculate an acceptance interval of length(x) */
  public abstract readonly lengthInterval: (
    n: number | FPInterval | number[] | FPVector
  ) => FPInterval;

  private readonly LogIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(
      this.constants().greaterThanZeroInterval,
      (n: number): FPInterval => {
        assert(this.kind === 'f32' || this.kind === 'f16');
        const abs_error = this.kind === 'f32' ? 2 ** -21 : 2 ** -7;
        if (n >= 0.5 && n <= 2.0) {
          return this.absoluteErrorInterval(Math.log(n), abs_error);
        }
        return this.ulpInterval(Math.log(n), 3);
      }
    ),
  };

  protected logIntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.LogIntervalOp);
  }

  /** Calculate an acceptance interval of log(x) */
  public abstract readonly logInterval: (x: number | FPInterval) => FPInterval;

  private readonly Log2IntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(
      this.constants().greaterThanZeroInterval,
      (n: number): FPInterval => {
        assert(this.kind === 'f32' || this.kind === 'f16');
        const abs_error = this.kind === 'f32' ? 2 ** -21 : 2 ** -7;
        if (n >= 0.5 && n <= 2.0) {
          return this.absoluteErrorInterval(Math.log2(n), abs_error);
        }
        return this.ulpInterval(Math.log2(n), 3);
      }
    ),
  };

  protected log2IntervalImpl(x: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(x), this.Log2IntervalOp);
  }

  /** Calculate an acceptance interval of log2(x) */
  public abstract readonly log2Interval: (x: number | FPInterval) => FPInterval;

  private readonly MaxIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      // If both of the inputs are subnormal, then either of the inputs can be returned
      if (this.isSubnormal(x) && this.isSubnormal(y)) {
        return this.correctlyRoundedInterval(
          this.spanIntervals(this.toInterval(x), this.toInterval(y))
        );
      }

      return this.correctlyRoundedInterval(Math.max(x, y));
    },
  };

  protected maxIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.MaxIntervalOp
    );
  }

  /** Calculate an acceptance interval of max(x, y) */
  public abstract readonly maxInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  private readonly MinIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      // If both of the inputs are subnormal, then either of the inputs can be returned
      if (this.isSubnormal(x) && this.isSubnormal(y)) {
        return this.correctlyRoundedInterval(
          this.spanIntervals(this.toInterval(x), this.toInterval(y))
        );
      }

      return this.correctlyRoundedInterval(Math.min(x, y));
    },
  };

  protected minIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.MinIntervalOp
    );
  }

  /** Calculate an acceptance interval of min(x, y) */
  public abstract readonly minInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  private readonly MixImpreciseIntervalOp: ScalarTripleToIntervalOp = {
    impl: (x: number, y: number, z: number): FPInterval => {
      // x + (y - x) * z =
      //  x + t, where t = (y - x) * z
      const t = this.multiplicationInterval(this.subtractionInterval(y, x), z);
      return this.additionInterval(x, t);
    },
  };

  protected mixImpreciseIntervalImpl(x: number, y: number, z: number): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.toInterval(z),
      this.MixImpreciseIntervalOp
    );
  }

  /** Calculate an acceptance interval of mix(x, y, z) using x + (y - x) * z */
  public abstract readonly mixImpreciseInterval: (x: number, y: number, z: number) => FPInterval;

  private readonly MixPreciseIntervalOp: ScalarTripleToIntervalOp = {
    impl: (x: number, y: number, z: number): FPInterval => {
      // x * (1.0 - z) + y * z =
      //   t + s, where t = x * (1.0 - z), s = y * z
      const t = this.multiplicationInterval(x, this.subtractionInterval(1.0, z));
      const s = this.multiplicationInterval(y, z);
      return this.additionInterval(t, s);
    },
  };

  protected mixPreciseIntervalImpl(x: number, y: number, z: number): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.toInterval(z),
      this.MixPreciseIntervalOp
    );
  }

  /** Calculate an acceptance interval of mix(x, y, z) using x * (1.0 - z) + y * z */
  public abstract readonly mixPreciseInterval: (x: number, y: number, z: number) => FPInterval;

  /** All acceptance interval functions for mix(x, y, z) */
  public abstract readonly mixIntervals: ScalarTripleToInterval[];

  protected modfIntervalImpl(n: number): { fract: FPInterval; whole: FPInterval } {
    const fract = this.correctlyRoundedInterval(n % 1.0);
    const whole = this.correctlyRoundedInterval(n - (n % 1.0));
    return { fract, whole };
  }

  /** Calculate an acceptance interval of modf(x) */
  public abstract readonly modfInterval: (n: number) => { fract: FPInterval; whole: FPInterval };

  private readonly MultiplicationInnerOp = {
    impl: (x: number, y: number): FPInterval => {
      return this.correctlyRoundedInterval(x * y);
    },
  };

  private readonly MultiplicationIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      return this.roundAndFlushScalarPairToInterval(x, y, this.MultiplicationInnerOp);
    },
  };

  protected multiplicationIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.MultiplicationIntervalOp
    );
  }

  /** Calculate an acceptance interval of x * y */
  public abstract readonly multiplicationInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  /**
   * @returns the vector result of multiplying the given vector by the given
   *          scalar
   */
  private multiplyVectorByScalar(v: number[], c: number | FPInterval): FPVector {
    return this.toVector(v.map(x => this.multiplicationInterval(x, c)));
  }

  protected multiplicationMatrixScalarIntervalImpl(mat: Array2D<number>, scalar: number): FPMatrix {
    const cols = mat.length;
    const rows = mat[0].length;
    return this.toMatrix(
      unflatten2DArray(
        flatten2DArray(mat).map(e => this.MultiplicationIntervalOp.impl(e, scalar)),
        cols,
        rows
      )
    );
  }

  /** Calculate an acceptance interval of x * y, when x is a matrix and y is a scalar */
  public abstract readonly multiplicationMatrixScalarInterval: (
    mat: Array2D<number>,
    scalar: number
  ) => FPMatrix;

  protected multiplicationScalarMatrixIntervalImpl(scalar: number, mat: Array2D<number>): FPMatrix {
    return this.multiplicationMatrixScalarIntervalImpl(mat, scalar);
  }

  /** Calculate an acceptance interval of x * y, when x is a scalar and y is a matrix */
  public abstract readonly multiplicationScalarMatrixInterval: (
    scalar: number,
    mat: Array2D<number>
  ) => FPMatrix;

  protected multiplicationMatrixMatrixIntervalImpl(
    mat_x: Array2D<number>,
    mat_y: Array2D<number>
  ): FPMatrix {
    const x_cols = mat_x.length;
    const x_rows = mat_x[0].length;
    const y_cols = mat_y.length;
    const y_rows = mat_y[0].length;
    assert(x_cols === y_rows, `'mat${x_cols}x${x_rows} * mat${y_cols}x${y_rows}' is not defined`);

    const x_transposed = this.transposeInterval(mat_x);

    const result: Array2D<FPInterval> = [...Array(y_cols)].map(_ => [...Array(x_rows)]);
    mat_y.forEach((y, i) => {
      x_transposed.forEach((x, j) => {
        result[i][j] = this.dotInterval(x, y);
      });
    });

    return result as FPMatrix;
  }

  /** Calculate an acceptance interval of x * y, when x is a matrix and y is a matrix */
  public abstract readonly multiplicationMatrixMatrixInterval: (
    mat_x: Array2D<number>,
    mat_y: Array2D<number>
  ) => FPMatrix;

  protected multiplicationMatrixVectorIntervalImpl(x: Array2D<number>, y: number[]): FPVector {
    const cols = x.length;
    const rows = x[0].length;
    assert(y.length === cols, `'mat${cols}x${rows} * vec${y.length}' is not defined`);

    return this.transposeInterval(x).map(e => this.dotInterval(e, y)) as FPVector;
  }

  /** Calculate an acceptance interval of x * y, when x is a matrix and y is a vector */
  public abstract readonly multiplicationMatrixVectorInterval: (
    x: Array2D<number>,
    y: number[]
  ) => FPVector;

  protected multiplicationVectorMatrixIntervalImpl(x: number[], y: Array2D<number>): FPVector {
    const cols = y.length;
    const rows = y[0].length;
    assert(x.length === rows, `'vec${x.length} * mat${cols}x${rows}' is not defined`);

    return y.map(e => this.dotInterval(x, e)) as FPVector;
  }

  /** Calculate an acceptance interval of x * y, when x is a vector and y is a matrix */
  public abstract readonly multiplicationVectorMatrixInterval: (
    x: number[],
    y: Array2D<number>
  ) => FPVector;

  private readonly NegationIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.correctlyRoundedInterval(-n);
    },
  };

  protected negationIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.NegationIntervalOp);
  }

  /** Calculate an acceptance interval of -x */
  public abstract readonly negationInterval: (n: number) => FPInterval;

  private readonly NormalizeIntervalOp: VectorToVectorOp = {
    impl: (n: number[]): FPVector => {
      const length = this.lengthInterval(n);
      return this.toVector(n.map(e => this.divisionInterval(e, length)));
    },
  };

  protected normalizeIntervalImpl(n: number[]): FPVector {
    return this.runVectorToVectorOp(this.toVector(n), this.NormalizeIntervalOp);
  }

  public abstract readonly normalizeInterval: (n: number[]) => FPVector;

  private readonly PowIntervalOp: ScalarPairToIntervalOp = {
    // pow(x, y) has no explicit domain restrictions, but inherits the x <= 0
    // domain restriction from log2(x). Invoking log2Interval(x) in impl will
    // enforce this, so there is no need to wrap the impl call here.
    impl: (x: number, y: number): FPInterval => {
      return this.exp2Interval(this.multiplicationInterval(y, this.log2Interval(x)));
    },
  };

  protected powIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.PowIntervalOp
    );
  }

  /** Calculate an acceptance interval of pow(x, y) */
  public abstract readonly powInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  private readonly RadiansIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.multiplicationInterval(n, 0.017453292519943295474);
    },
  };

  protected radiansIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.RadiansIntervalOp);
  }

  /** Calculate an acceptance interval of radians(x) */
  public abstract readonly radiansInterval: (n: number) => FPInterval;

  private readonly ReflectIntervalOp: VectorPairToVectorOp = {
    impl: (x: number[], y: number[]): FPVector => {
      assert(
        x.length === y.length,
        `ReflectIntervalOp received x (${x}) and y (${y}) with different numbers of elements`
      );

      // reflect(x, y) = x - 2.0 * dot(x, y) * y
      //               = x - t * y, t = 2.0 * dot(x, y)
      // x = incident vector
      // y = normal of reflecting surface
      const t = this.multiplicationInterval(2.0, this.dotInterval(x, y));
      const rhs = this.multiplyVectorByScalar(y, t);
      return this.runScalarPairToIntervalOpVectorComponentWise(
        this.toVector(x),
        rhs,
        this.SubtractionIntervalOp
      );
    },
  };

  protected reflectIntervalImpl(x: number[], y: number[]): FPVector {
    assert(
      x.length === y.length,
      `reflect is only defined for vectors with the same number of elements`
    );
    return this.runVectorPairToVectorOp(this.toVector(x), this.toVector(y), this.ReflectIntervalOp);
  }

  /** Calculate an acceptance interval of reflect(x, y) */
  public abstract readonly reflectInterval: (x: number[], y: number[]) => FPVector;

  /**
   * refract is a singular function in the sense that it is the only builtin that
   * takes in (FPVector, FPVector, F32) and returns FPVector and is basically
   * defined in terms of other functions.
   *
   * Instead of implementing all the framework code to integrate it with its
   * own operation type, etc, it instead has a bespoke implementation that is a
   * composition of other builtin functions that use the framework.
   */
  protected refractIntervalImpl(i: number[], s: number[], r: number): FPVector {
    assert(
      i.length === s.length,
      `refract is only defined for vectors with the same number of elements`
    );

    const r_squared = this.multiplicationInterval(r, r);
    const dot = this.dotInterval(s, i);
    const dot_squared = this.multiplicationInterval(dot, dot);
    const one_minus_dot_squared = this.subtractionInterval(1, dot_squared);
    const k = this.subtractionInterval(
      1.0,
      this.multiplicationInterval(r_squared, one_minus_dot_squared)
    );

    if (!k.isFinite() || k.containsZeroOrSubnormals()) {
      // There is a discontinuity at k == 0, due to sqrt(k) being calculated, so exiting early
      return this.constants().unboundedVector[this.toVector(i).length];
    }

    if (k.end < 0.0) {
      // if k is negative, then the zero vector is the valid response
      return this.constants().zeroVector[this.toVector(i).length];
    }

    const dot_times_r = this.multiplicationInterval(dot, r);
    const k_sqrt = this.sqrtInterval(k);
    const t = this.additionInterval(dot_times_r, k_sqrt); // t = r * dot(i, s) + sqrt(k)

    return this.runScalarPairToIntervalOpVectorComponentWise(
      this.multiplyVectorByScalar(i, r),
      this.multiplyVectorByScalar(s, t),
      this.SubtractionIntervalOp
    ); // (i * r) - (s * t)
  }

  /** Calculate acceptance interval vectors of reflect(i, s, r) */
  public abstract readonly refractInterval: (i: number[], s: number[], r: number) => FPVector;

  private readonly RemainderIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      // x % y = x - y * trunc(x/y)
      return this.subtractionInterval(
        x,
        this.multiplicationInterval(y, this.truncInterval(this.divisionInterval(x, y)))
      );
    },
  };

  /** Calculate an acceptance interval for x % y */
  protected remainderIntervalImpl(x: number, y: number): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.RemainderIntervalOp
    );
  }

  /** Calculate an acceptance interval for x % y */
  public abstract readonly remainderInterval: (x: number, y: number) => FPInterval;

  private readonly RoundIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      const k = Math.floor(n);
      const diff_before = n - k;
      const diff_after = k + 1 - n;
      if (diff_before < diff_after) {
        return this.correctlyRoundedInterval(k);
      } else if (diff_before > diff_after) {
        return this.correctlyRoundedInterval(k + 1);
      }

      // n is in the middle of two integers.
      // The tie breaking rule is 'k if k is even, k + 1 if k is odd'
      if (k % 2 === 0) {
        return this.correctlyRoundedInterval(k);
      }
      return this.correctlyRoundedInterval(k + 1);
    },
  };

  protected roundIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.RoundIntervalOp);
  }

  /** Calculate an acceptance interval of round(x) */
  public abstract readonly roundInterval: (n: number) => FPInterval;

  /**
   * The definition of saturate does not specify which version of clamp to use.
   * Using min-max here, since it has wider acceptance intervals, that include
   * all of median's.
   */
  protected saturateIntervalImpl(n: number): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(n),
      this.toInterval(0.0),
      this.toInterval(1.0),
      this.ClampMinMaxIntervalOp
    );
  }

  /*** Calculate an acceptance interval of saturate(n) as clamp(n, 0.0, 1.0) */
  public abstract readonly saturateInterval: (n: number) => FPInterval;

  private readonly SignIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      if (n > 0.0) {
        return this.correctlyRoundedInterval(1.0);
      }
      if (n < 0.0) {
        return this.correctlyRoundedInterval(-1.0);
      }

      return this.correctlyRoundedInterval(0.0);
    },
  };

  protected signIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.SignIntervalOp);
  }

  /** Calculate an acceptance interval of sign(x) */
  public abstract readonly signInterval: (n: number) => FPInterval;

  private readonly SinIntervalOp: ScalarToIntervalOp = {
    impl: this.limitScalarToIntervalDomain(
      this.constants().negPiToPiInterval,
      (n: number): FPInterval => {
        assert(this.kind === 'f32' || this.kind === 'f16');
        const abs_error = this.kind === 'f32' ? 2 ** -11 : 2 ** -7;
        return this.absoluteErrorInterval(Math.sin(n), abs_error);
      }
    ),
  };

  protected sinIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.SinIntervalOp);
  }

  /** Calculate an acceptance interval of sin(x) */
  public abstract readonly sinInterval: (n: number) => FPInterval;

  private readonly SinhIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      // sinh(x) = (exp(x) - exp(-x)) * 0.5
      const minus_n = this.negationInterval(n);
      return this.multiplicationInterval(
        this.subtractionInterval(this.expInterval(n), this.expInterval(minus_n)),
        0.5
      );
    },
  };

  protected sinhIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.SinhIntervalOp);
  }

  /** Calculate an acceptance interval of sinh(x) */
  public abstract readonly sinhInterval: (n: number) => FPInterval;

  private readonly SmoothStepOp: ScalarTripleToIntervalOp = {
    impl: (low: number, high: number, x: number): FPInterval => {
      // For clamp(foo, 0.0, 1.0) the different implementations of clamp provide
      // the same value, so arbitrarily picking the minmax version to use.
      // t = clamp((x - low) / (high - low), 0.0, 1.0)
      // prettier-ignore
      const t = this.clampMedianInterval(
        this.divisionInterval(
          this.subtractionInterval(x, low),
          this.subtractionInterval(high, low)),
        0.0,
        1.0);
      // Inherited from t * t * (3.0 - 2.0 * t)
      // prettier-ignore
      return this.multiplicationInterval(
        t,
        this.multiplicationInterval(t,
          this.subtractionInterval(3.0,
            this.multiplicationInterval(2.0, t))));
    },
  };

  protected smoothStepIntervalImpl(low: number, high: number, x: number): FPInterval {
    return this.runScalarTripleToIntervalOp(
      this.toInterval(low),
      this.toInterval(high),
      this.toInterval(x),
      this.SmoothStepOp
    );
  }

  /** Calculate an acceptance interval of smoothStep(low, high, x) */
  public abstract readonly smoothStepInterval: (low: number, high: number, x: number) => FPInterval;

  private readonly SqrtIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.divisionInterval(1.0, this.inverseSqrtInterval(n));
    },
  };

  protected sqrtIntervalImpl(n: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.SqrtIntervalOp);
  }

  /** Calculate an acceptance interval of sqrt(x) */
  public abstract readonly sqrtInterval: (n: number | FPInterval) => FPInterval;

  private readonly StepIntervalOp: ScalarPairToIntervalOp = {
    impl: (edge: number, x: number): FPInterval => {
      if (edge <= x) {
        return this.correctlyRoundedInterval(1.0);
      }
      return this.correctlyRoundedInterval(0.0);
    },
  };

  protected stepIntervalImpl(edge: number, x: number): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(edge),
      this.toInterval(x),
      this.StepIntervalOp
    );
  }

  /**
   * Calculate an acceptance 'interval' for step(edge, x)
   *
   * step only returns two possible values, so its interval requires special
   * interpretation in CTS tests.
   * This interval will be one of four values: [0, 0], [0, 1], [1, 1] & [-∞, +∞].
   * [0, 0] and [1, 1] indicate that the correct answer in point they encapsulate.
   * [0, 1] should not be treated as a span, i.e. 0.1 is acceptable, but instead
   * indicate either 0.0 or 1.0 are acceptable answers.
   * [-∞, +∞] is treated as unbounded interval, since an unbounded or
   * infinite value was passed in.
   */
  public abstract readonly stepInterval: (edge: number, x: number) => FPInterval;

  private readonly SubtractionIntervalOp: ScalarPairToIntervalOp = {
    impl: (x: number, y: number): FPInterval => {
      return this.correctlyRoundedInterval(x - y);
    },
  };

  protected subtractionIntervalImpl(x: number | FPInterval, y: number | FPInterval): FPInterval {
    return this.runScalarPairToIntervalOp(
      this.toInterval(x),
      this.toInterval(y),
      this.SubtractionIntervalOp
    );
  }

  /** Calculate an acceptance interval of x - y */
  public abstract readonly subtractionInterval: (
    x: number | FPInterval,
    y: number | FPInterval
  ) => FPInterval;

  protected subtractionMatrixMatrixIntervalImpl(x: Array2D<number>, y: Array2D<number>): FPMatrix {
    return this.runScalarPairToIntervalOpMatrixComponentWise(
      this.toMatrix(x),
      this.toMatrix(y),
      this.SubtractionIntervalOp
    );
  }

  /** Calculate an acceptance interval of x - y, when x and y are matrices */
  public abstract readonly subtractionMatrixMatrixInterval: (
    x: Array2D<number>,
    y: Array2D<number>
  ) => FPMatrix;

  private readonly TanIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.divisionInterval(this.sinInterval(n), this.cosInterval(n));
    },
  };

  protected tanIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.TanIntervalOp);
  }

  /** Calculate an acceptance interval of tan(x) */
  public abstract readonly tanInterval: (n: number) => FPInterval;

  private readonly TanhIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.divisionInterval(this.sinhInterval(n), this.coshInterval(n));
    },
  };

  protected tanhIntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.TanhIntervalOp);
  }

  /** Calculate an acceptance interval of tanh(x) */
  public abstract readonly tanhInterval: (n: number) => FPInterval;

  private readonly TransposeIntervalOp: MatrixToMatrixOp = {
    impl: (m: Array2D<number>): FPMatrix => {
      const num_cols = m.length;
      const num_rows = m[0].length;
      const result: Array2D<FPInterval> = [...Array(num_rows)].map(_ => [...Array(num_cols)]);

      for (let i = 0; i < num_cols; i++) {
        for (let j = 0; j < num_rows; j++) {
          result[j][i] = this.correctlyRoundedInterval(m[i][j]);
        }
      }
      return this.toMatrix(result);
    },
  };

  protected transposeIntervalImpl(m: Array2D<number>): FPMatrix {
    return this.runMatrixToMatrixOp(this.toMatrix(m), this.TransposeIntervalOp);
  }

  /** Calculate an acceptance interval of transpose(m) */
  public abstract readonly transposeInterval: (m: Array2D<number>) => FPMatrix;

  private readonly TruncIntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      return this.correctlyRoundedInterval(Math.trunc(n));
    },
  };

  protected truncIntervalImpl(n: number | FPInterval): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.TruncIntervalOp);
  }

  /** Calculate an acceptance interval of trunc(x) */
  public abstract readonly truncInterval: (n: number | FPInterval) => FPInterval;
}

// Pre-defined values that get used multiple times in _constants' initializers. Cannot use FPTraits members, since this
// executes before they are defined.
const kF32UnboundedInterval = new FPInterval(
  'f32',
  Number.NEGATIVE_INFINITY,
  Number.POSITIVE_INFINITY
);
const kF32ZeroInterval = new FPInterval('f32', 0);

class F32Traits extends FPTraits {
  private static _constants: FPConstants = {
    positive: {
      min: kValue.f32.positive.min,
      max: kValue.f32.positive.max,
      infinity: kValue.f32.infinity.positive,
      nearest_max: kValue.f32.positive.nearest_max,
      less_than_one: kValue.f32.positive.less_than_one,
      subnormal: {
        min: kValue.f32.subnormal.positive.min,
        max: kValue.f32.subnormal.positive.max,
      },
      pi: {
        whole: kValue.f32.positive.pi.whole,
        three_quarters: kValue.f32.positive.pi.three_quarters,
        half: kValue.f32.positive.pi.half,
        third: kValue.f32.positive.pi.third,
        quarter: kValue.f32.positive.pi.quarter,
        sixth: kValue.f32.positive.pi.sixth,
      },
      e: kValue.f32.positive.e,
    },
    negative: {
      min: kValue.f32.negative.min,
      max: kValue.f32.negative.max,
      infinity: kValue.f32.infinity.negative,
      nearest_min: kValue.f32.negative.nearest_min,
      less_than_one: kValue.f32.negative.less_than_one,
      subnormal: {
        min: kValue.f32.subnormal.negative.min,
        max: kValue.f32.subnormal.negative.max,
      },
      pi: {
        whole: kValue.f32.negative.pi.whole,
        three_quarters: kValue.f32.negative.pi.three_quarters,
        half: kValue.f32.negative.pi.half,
        third: kValue.f32.negative.pi.third,
        quarter: kValue.f32.negative.pi.quarter,
        sixth: kValue.f32.negative.pi.sixth,
      },
    },
    unboundedInterval: kF32UnboundedInterval,
    zeroInterval: kF32ZeroInterval,
    // Have to use the constants.ts values here, because values defined in the
    // initializer cannot be referenced in the initializer
    negPiToPiInterval: new FPInterval(
      'f32',
      kValue.f32.negative.pi.whole,
      kValue.f32.positive.pi.whole
    ),
    greaterThanZeroInterval: new FPInterval(
      'f32',
      kValue.f32.subnormal.positive.min,
      kValue.f32.positive.max
    ),
    zeroVector: {
      2: [kF32ZeroInterval, kF32ZeroInterval],
      3: [kF32ZeroInterval, kF32ZeroInterval, kF32ZeroInterval],
      4: [kF32ZeroInterval, kF32ZeroInterval, kF32ZeroInterval, kF32ZeroInterval],
    },
    unboundedVector: {
      2: [kF32UnboundedInterval, kF32UnboundedInterval],
      3: [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
      4: [
        kF32UnboundedInterval,
        kF32UnboundedInterval,
        kF32UnboundedInterval,
        kF32UnboundedInterval,
      ],
    },
    unboundedMatrix: {
      2: {
        2: [
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        3: [
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        4: [
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
        ],
      },
      3: {
        2: [
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        3: [
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        4: [
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
        ],
      },
      4: {
        2: [
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        3: [
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
          [kF32UnboundedInterval, kF32UnboundedInterval, kF32UnboundedInterval],
        ],
        4: [
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
          [
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
            kF32UnboundedInterval,
          ],
        ],
      },
    },
  };

  public constructor() {
    super('f32');
  }

  public constants(): FPConstants {
    return F32Traits._constants;
  }

  // Utilities - Overrides
  public readonly quantize = quantizeToF32;
  public readonly correctlyRounded = correctlyRoundedF32;
  public readonly isFinite = isFiniteF32;
  public readonly isSubnormal = isSubnormalNumberF32;
  public readonly flushSubnormal = flushSubnormalNumberF32;
  public readonly oneULP = oneULPF32;
  public readonly scalarBuilder = f32;

  // Framework - Fundamental Error Intervals - Overrides
  public readonly absoluteErrorInterval = this.absoluteErrorIntervalImpl.bind(this);
  public readonly correctlyRoundedInterval = this.correctlyRoundedIntervalImpl.bind(this);
  public readonly correctlyRoundedMatrix = this.correctlyRoundedMatrixImpl.bind(this);
  public readonly ulpInterval = this.ulpIntervalImpl.bind(this);

  // Framework - API - Overrides
  public readonly absInterval = this.absIntervalImpl.bind(this);
  public readonly acosInterval = this.acosIntervalImpl.bind(this);
  public readonly acoshAlternativeInterval = this.acoshAlternativeIntervalImpl.bind(this);
  public readonly acoshPrimaryInterval = this.acoshPrimaryIntervalImpl.bind(this);
  public readonly acoshIntervals = [this.acoshAlternativeInterval, this.acoshPrimaryInterval];
  public readonly additionInterval = this.additionIntervalImpl.bind(this);
  public readonly additionMatrixMatrixInterval = this.additionMatrixMatrixIntervalImpl.bind(this);
  public readonly asinInterval = this.asinIntervalImpl.bind(this);
  public readonly asinhInterval = this.asinhIntervalImpl.bind(this);
  public readonly atanInterval = this.atanIntervalImpl.bind(this);
  public readonly atan2Interval = this.atan2IntervalImpl.bind(this);
  public readonly atanhInterval = this.atanhIntervalImpl.bind(this);
  public readonly ceilInterval = this.ceilIntervalImpl.bind(this);
  public readonly clampMedianInterval = this.clampMedianIntervalImpl.bind(this);
  public readonly clampMinMaxInterval = this.clampMinMaxIntervalImpl.bind(this);
  public readonly clampIntervals = [this.clampMedianInterval, this.clampMinMaxInterval];
  public readonly cosInterval = this.cosIntervalImpl.bind(this);
  public readonly coshInterval = this.coshIntervalImpl.bind(this);
  public readonly crossInterval = this.crossIntervalImpl.bind(this);
  public readonly degreesInterval = this.degreesIntervalImpl.bind(this);
  public readonly determinantInterval = this.determinantIntervalImpl.bind(this);
  public readonly distanceInterval = this.distanceIntervalImpl.bind(this);
  public readonly divisionInterval = this.divisionIntervalImpl.bind(this);
  public readonly dotInterval = this.dotIntervalImpl.bind(this);
  public readonly expInterval = this.expIntervalImpl.bind(this);
  public readonly exp2Interval = this.exp2IntervalImpl.bind(this);
  public readonly faceForwardIntervals = this.faceForwardIntervalsImpl.bind(this);
  public readonly floorInterval = this.floorIntervalImpl.bind(this);
  public readonly fmaInterval = this.fmaIntervalImpl.bind(this);
  public readonly fractInterval = this.fractIntervalImpl.bind(this);
  public readonly inverseSqrtInterval = this.inverseSqrtIntervalImpl.bind(this);
  public readonly ldexpInterval = this.ldexpIntervalImpl.bind(this);
  public readonly lengthInterval = this.lengthIntervalImpl.bind(this);
  public readonly logInterval = this.logIntervalImpl.bind(this);
  public readonly log2Interval = this.log2IntervalImpl.bind(this);
  public readonly maxInterval = this.maxIntervalImpl.bind(this);
  public readonly minInterval = this.minIntervalImpl.bind(this);
  public readonly mixImpreciseInterval = this.mixImpreciseIntervalImpl.bind(this);
  public readonly mixPreciseInterval = this.mixPreciseIntervalImpl.bind(this);
  public readonly mixIntervals = [this.mixImpreciseInterval, this.mixPreciseInterval];
  public readonly modfInterval = this.modfIntervalImpl.bind(this);
  public readonly multiplicationInterval = this.multiplicationIntervalImpl.bind(this);
  public readonly multiplicationMatrixMatrixInterval = this.multiplicationMatrixMatrixIntervalImpl.bind(
    this
  );
  public readonly multiplicationMatrixScalarInterval = this.multiplicationMatrixScalarIntervalImpl.bind(
    this
  );
  public readonly multiplicationScalarMatrixInterval = this.multiplicationScalarMatrixIntervalImpl.bind(
    this
  );
  public readonly multiplicationMatrixVectorInterval = this.multiplicationMatrixVectorIntervalImpl.bind(
    this
  );
  public readonly multiplicationVectorMatrixInterval = this.multiplicationVectorMatrixIntervalImpl.bind(
    this
  );
  public readonly negationInterval = this.negationIntervalImpl.bind(this);
  public readonly normalizeInterval = this.normalizeIntervalImpl.bind(this);
  public readonly powInterval = this.powIntervalImpl.bind(this);
  public readonly radiansInterval = this.radiansIntervalImpl.bind(this);
  public readonly reflectInterval = this.reflectIntervalImpl.bind(this);
  public readonly refractInterval = this.refractIntervalImpl.bind(this);
  public readonly remainderInterval = this.remainderIntervalImpl.bind(this);
  public readonly roundInterval = this.roundIntervalImpl.bind(this);
  public readonly saturateInterval = this.saturateIntervalImpl.bind(this);
  public readonly signInterval = this.signIntervalImpl.bind(this);
  public readonly sinInterval = this.sinIntervalImpl.bind(this);
  public readonly sinhInterval = this.sinhIntervalImpl.bind(this);
  public readonly smoothStepInterval = this.smoothStepIntervalImpl.bind(this);
  public readonly sqrtInterval = this.sqrtIntervalImpl.bind(this);
  public readonly stepInterval = this.stepIntervalImpl.bind(this);
  public readonly subtractionInterval = this.subtractionIntervalImpl.bind(this);
  public readonly subtractionMatrixMatrixInterval = this.subtractionMatrixMatrixIntervalImpl.bind(
    this
  );
  public readonly tanInterval = this.tanIntervalImpl.bind(this);
  public readonly tanhInterval = this.tanhIntervalImpl.bind(this);
  public readonly transposeInterval = this.transposeIntervalImpl.bind(this);
  public readonly truncInterval = this.truncIntervalImpl.bind(this);

  // Framework - Cases

  // U32 -> Interval is used for testing f32 specific unpack* functions
  /**
   * @returns a Case for the param and the interval generator provided.
   * The Case will use an interval comparator for matching results.
   * @param param the param to pass in
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  private makeU32ToVectorCase(
    param: number,
    filter: IntervalFilter,
    ...ops: ScalarToVector[]
  ): Case | undefined {
    param = Math.trunc(param);

    const vectors = ops.map(o => o(param));
    if (filter === 'finite' && vectors.some(v => !v.every(e => e.isFinite()))) {
      return undefined;
    }
    return {
      input: u32(param),
      expected: anyOf(...vectors),
    };
  }

  /**
   * @returns an array of Cases for operations over a range of inputs
   * @param params array of inputs to try
   * @param filter what interval filtering to apply
   * @param ops callbacks that implement generating an acceptance interval
   */
  public generateU32ToIntervalCases(
    params: number[],
    filter: IntervalFilter,
    ...ops: ScalarToVector[]
  ): Case[] {
    return params.reduce((cases, e) => {
      const c = this.makeU32ToVectorCase(e, filter, ...ops);
      if (c !== undefined) {
        cases.push(c);
      }
      return cases;
    }, new Array<Case>());
  }

  // Framework - API

  /**
   * Once-allocated ArrayBuffer/views to avoid overhead of allocation when
   * converting between numeric formats
   *
   * unpackData* is shared between all the unpack*Interval functions, so to
   * avoid re-entrancy problems, they should not call each other or themselves
   * directly or indirectly.
   */
  private readonly unpackData = new ArrayBuffer(4);
  private readonly unpackDataU32 = new Uint32Array(this.unpackData);
  private readonly unpackDataU16 = new Uint16Array(this.unpackData);
  private readonly unpackDataU8 = new Uint8Array(this.unpackData);
  private readonly unpackDataI16 = new Int16Array(this.unpackData);
  private readonly unpackDataI8 = new Int8Array(this.unpackData);
  private readonly unpackDataF16 = new Float16Array(this.unpackData);

  private unpack2x16floatIntervalImpl(n: number): FPVector {
    assert(
      n >= kValue.u32.min && n <= kValue.u32.max,
      'unpack2x16floatInterval only accepts values on the bounds of u32'
    );
    this.unpackDataU32[0] = n;
    if (this.unpackDataF16.some(f => !isFiniteF16(f))) {
      return [this.constants().unboundedInterval, this.constants().unboundedInterval];
    }

    const result: FPVector = [
      this.quantizeToF16Interval(this.unpackDataF16[0]),
      this.quantizeToF16Interval(this.unpackDataF16[1]),
    ];

    if (result.some(r => !r.isFinite())) {
      return [this.constants().unboundedInterval, this.constants().unboundedInterval];
    }
    return result;
  }

  /** Calculate an acceptance interval vector for unpack2x16float(x) */
  public readonly unpack2x16floatInterval = this.unpack2x16floatIntervalImpl.bind(this);

  private unpack2x16snormIntervalImpl(n: number): FPVector {
    assert(
      n >= kValue.u32.min && n <= kValue.u32.max,
      'unpack2x16snormInterval only accepts values on the bounds of u32'
    );
    const op = (n: number): FPInterval => {
      return this.ulpInterval(Math.max(n / 32767, -1), 3);
    };

    this.unpackDataU32[0] = n;
    return [op(this.unpackDataI16[0]), op(this.unpackDataI16[1])];
  }

  /** Calculate an acceptance interval vector for unpack2x16snorm(x) */
  public readonly unpack2x16snormInterval = this.unpack2x16snormIntervalImpl.bind(this);

  private unpack2x16unormIntervalImpl(n: number): FPVector {
    assert(
      n >= kValue.u32.min && n <= kValue.u32.max,
      'unpack2x16unormInterval only accepts values on the bounds of u32'
    );
    const op = (n: number): FPInterval => {
      return this.ulpInterval(n / 65535, 3);
    };

    this.unpackDataU32[0] = n;
    return [op(this.unpackDataU16[0]), op(this.unpackDataU16[1])];
  }

  /** Calculate an acceptance interval vector for unpack2x16unorm(x) */
  public readonly unpack2x16unormInterval = this.unpack2x16unormIntervalImpl.bind(this);

  private unpack4x8snormIntervalImpl(n: number): FPVector {
    assert(
      n >= kValue.u32.min && n <= kValue.u32.max,
      'unpack4x8snormInterval only accepts values on the bounds of u32'
    );
    const op = (n: number): FPInterval => {
      return this.ulpInterval(Math.max(n / 127, -1), 3);
    };
    this.unpackDataU32[0] = n;
    return [
      op(this.unpackDataI8[0]),
      op(this.unpackDataI8[1]),
      op(this.unpackDataI8[2]),
      op(this.unpackDataI8[3]),
    ];
  }

  /** Calculate an acceptance interval vector for unpack4x8snorm(x) */
  public readonly unpack4x8snormInterval = this.unpack4x8snormIntervalImpl.bind(this);

  private unpack4x8unormIntervalImpl(n: number): FPVector {
    assert(
      n >= kValue.u32.min && n <= kValue.u32.max,
      'unpack4x8unormInterval only accepts values on the bounds of u32'
    );
    const op = (n: number): FPInterval => {
      return this.ulpInterval(n / 255, 3);
    };

    this.unpackDataU32[0] = n;
    return [
      op(this.unpackDataU8[0]),
      op(this.unpackDataU8[1]),
      op(this.unpackDataU8[2]),
      op(this.unpackDataU8[3]),
    ];
  }

  /** Calculate an acceptance interval vector for unpack4x8unorm(x) */
  public readonly unpack4x8unormInterval = this.unpack4x8unormIntervalImpl.bind(this);

  private readonly QuantizeToF16IntervalOp: ScalarToIntervalOp = {
    impl: (n: number): FPInterval => {
      const rounded = correctlyRoundedF16(n);
      const flushed = addFlushedIfNeededF16(rounded);
      return this.spanIntervals(...flushed.map(f => this.toInterval(f)));
    },
  };

  protected quantizeToF16IntervalImpl(n: number): FPInterval {
    return this.runScalarToIntervalOp(this.toInterval(n), this.QuantizeToF16IntervalOp);
  }

  /** Calculate an acceptance interval of quantizeToF16(x) */
  public readonly quantizeToF16Interval = this.quantizeToF16IntervalImpl.bind(this);
}

// Pre-defined values that get used multiple times in _constants' initializers. Cannot use FPTraits members, since this
// executes before they are defined.
const kAbstractUnboundedInterval = new FPInterval(
  'abstract',
  Number.NEGATIVE_INFINITY,
  Number.POSITIVE_INFINITY
);
const kAbstractZeroInterval = new FPInterval('abstract', 0);

// This is implementation is incomplete
class FPAbstractTraits extends FPTraits {
  private static _constants: FPConstants = {
    positive: {
      min: kValue.f64.positive.min,
      max: kValue.f64.positive.max,
      infinity: kValue.f64.infinity.positive,
      nearest_max: kValue.f64.positive.nearest_max,
      less_than_one: kValue.f64.positive.less_than_one,
      subnormal: {
        min: kValue.f64.subnormal.positive.min,
        max: kValue.f64.subnormal.positive.max,
      },
      pi: {
        whole: kValue.f64.positive.pi.whole,
        three_quarters: kValue.f64.positive.pi.three_quarters,
        half: kValue.f64.positive.pi.half,
        third: kValue.f64.positive.pi.third,
        quarter: kValue.f64.positive.pi.quarter,
        sixth: kValue.f64.positive.pi.sixth,
      },
      e: kValue.f64.positive.e,
    },
    negative: {
      min: kValue.f64.negative.min,
      max: kValue.f64.negative.max,
      infinity: kValue.f64.infinity.negative,
      nearest_min: kValue.f64.negative.nearest_min,
      less_than_one: kValue.f64.negative.less_than_one,
      subnormal: {
        min: kValue.f64.subnormal.negative.min,
        max: kValue.f64.subnormal.negative.max,
      },
      pi: {
        whole: kValue.f64.negative.pi.whole,
        three_quarters: kValue.f64.negative.pi.three_quarters,
        half: kValue.f64.negative.pi.half,
        third: kValue.f64.negative.pi.third,
        quarter: kValue.f64.negative.pi.quarter,
        sixth: kValue.f64.negative.pi.sixth,
      },
    },
    unboundedInterval: kAbstractUnboundedInterval,
    zeroInterval: kAbstractZeroInterval,
    // Have to use the constants.ts values here, because values defined in the
    // initializer cannot be referenced in the initializer
    negPiToPiInterval: new FPInterval(
      'abstract',
      kValue.f64.negative.pi.whole,
      kValue.f64.positive.pi.whole
    ),
    greaterThanZeroInterval: new FPInterval(
      'abstract',
      kValue.f64.subnormal.positive.min,
      kValue.f64.positive.max
    ),
    zeroVector: {
      2: [kAbstractZeroInterval, kAbstractZeroInterval],
      3: [kAbstractZeroInterval, kAbstractZeroInterval, kAbstractZeroInterval],
      4: [
        kAbstractZeroInterval,
        kAbstractZeroInterval,
        kAbstractZeroInterval,
        kAbstractZeroInterval,
      ],
    },
    unboundedVector: {
      2: [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
      3: [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
      4: [
        kAbstractUnboundedInterval,
        kAbstractUnboundedInterval,
        kAbstractUnboundedInterval,
        kAbstractUnboundedInterval,
      ],
    },
    unboundedMatrix: {
      2: {
        2: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        3: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        4: [
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
        ],
      },
      3: {
        2: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        3: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        4: [
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
        ],
      },
      4: {
        2: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        3: [
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
          [kAbstractUnboundedInterval, kAbstractUnboundedInterval, kAbstractUnboundedInterval],
        ],
        4: [
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
          [
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
            kAbstractUnboundedInterval,
          ],
        ],
      },
    },
  };

  public constructor() {
    super('abstract');
  }

  public constants(): FPConstants {
    return FPAbstractTraits._constants;
  }

  // Utilities - Overrides
  // number is represented as a f64 internally, so all number values are already
  // quantized to f64
  public readonly quantize = (n: number) => {
    return n;
  };
  public readonly correctlyRounded = correctlyRoundedF64;
  public readonly isFinite = Number.isFinite;
  public readonly isSubnormal = isSubnormalNumberF64;
  public readonly flushSubnormal = flushSubnormalNumberF64;
  public readonly oneULP = oneULPF64;
  public readonly scalarBuilder = abstractFloat;

  // Framework - Fundamental Error Intervals - Overrides
  public readonly absoluteErrorInterval = this.unboundedAbsoluteErrorInterval.bind(this);
  public readonly correctlyRoundedInterval = this.correctlyRoundedIntervalImpl.bind(this);
  public readonly correctlyRoundedMatrix = this.correctlyRoundedMatrixImpl.bind(this);
  public readonly ulpInterval = this.unboundedUlpInterval.bind(this);

  // Framework - API - Overrides
  public readonly absInterval = this.absIntervalImpl.bind(this);
  public readonly acosInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly acoshAlternativeInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly acoshPrimaryInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly acoshIntervals = [this.acoshAlternativeInterval, this.acoshPrimaryInterval];
  public readonly additionInterval = this.additionIntervalImpl.bind(this);
  public readonly additionMatrixMatrixInterval = this.additionMatrixMatrixIntervalImpl.bind(this);
  public readonly asinInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly asinhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly atanInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly atan2Interval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly atanhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly ceilInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly clampMedianInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly clampMinMaxInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly clampIntervals = [this.clampMedianInterval, this.clampMinMaxInterval];
  public readonly cosInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly coshInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly crossInterval = this.unimplementedVectorPairToVector.bind(this);
  public readonly degreesInterval = this.degreesIntervalImpl.bind(this);
  public readonly determinantInterval = this.unimplementedMatrixToInterval.bind(this);
  public readonly distanceInterval = this.unimplementedDistance.bind(this);
  public readonly divisionInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly dotInterval = this.unimplementedVectorPairToInterval.bind(this);
  public readonly expInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly exp2Interval = this.unimplementedScalarToInterval.bind(this);
  public readonly faceForwardIntervals = this.unimplementedFaceForward.bind(this);
  public readonly floorInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly fmaInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly fractInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly inverseSqrtInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly ldexpInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly lengthInterval = this.unimplementedLength.bind(this);
  public readonly logInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly log2Interval = this.unimplementedScalarToInterval.bind(this);
  public readonly maxInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly minInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly mixImpreciseInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly mixPreciseInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly mixIntervals = [this.mixImpreciseInterval, this.mixPreciseInterval];
  public readonly modfInterval = this.unimplementedModf.bind(this);
  public readonly multiplicationInterval = this.multiplicationIntervalImpl.bind(this);
  public readonly multiplicationMatrixMatrixInterval = this.unimplementedMatrixPairToMatrix.bind(
    this
  );
  public readonly multiplicationMatrixScalarInterval = this.unimplementedMatrixScalarToMatrix.bind(
    this
  );
  public readonly multiplicationScalarMatrixInterval = this.unimplementedScalarMatrixToMatrix.bind(
    this
  );
  public readonly multiplicationMatrixVectorInterval = this.unimplementedMatrixVectorToVector.bind(
    this
  );
  public readonly multiplicationVectorMatrixInterval = this.unimplementedVectorMatrixToVector.bind(
    this
  );
  public readonly negationInterval = this.negationIntervalImpl.bind(this);
  public readonly normalizeInterval = this.unimplementedVectorToVector.bind(this);
  public readonly powInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly quantizeToF16Interval = this.unimplementedScalarToInterval.bind(this);
  public readonly radiansInterval = this.radiansIntervalImpl.bind(this);
  public readonly reflectInterval = this.unimplementedVectorPairToVector.bind(this);
  public readonly refractInterval = this.unimplementedRefract.bind(this);
  public readonly remainderInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly roundInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly saturateInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly signInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly sinInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly sinhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly smoothStepInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly sqrtInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly stepInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly subtractionInterval = this.subtractionIntervalImpl.bind(this);
  public readonly subtractionMatrixMatrixInterval = this.subtractionMatrixMatrixIntervalImpl.bind(
    this
  );
  public readonly tanInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly tanhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly transposeInterval = this.transposeIntervalImpl.bind(this);
  public readonly truncInterval = this.truncIntervalImpl.bind(this);
}

// Pre-defined values that get used multiple times in _constants' initializers. Cannot use FPTraits members, since this
// executes before they are defined.
const kF16UnboundedInterval = new FPInterval(
  'f16',
  Number.NEGATIVE_INFINITY,
  Number.POSITIVE_INFINITY
);
const kF16ZeroInterval = new FPInterval('f16', 0);

// This is implementation is incomplete
class F16Traits extends FPTraits {
  private static _constants: FPConstants = {
    positive: {
      min: kValue.f16.positive.min,
      max: kValue.f16.positive.max,
      infinity: kValue.f16.infinity.positive,
      nearest_max: kValue.f16.positive.nearest_max,
      less_than_one: kValue.f16.positive.less_than_one,
      subnormal: {
        min: kValue.f16.subnormal.positive.min,
        max: kValue.f16.subnormal.positive.max,
      },
      pi: {
        whole: kValue.f16.positive.pi.whole,
        three_quarters: kValue.f16.positive.pi.three_quarters,
        half: kValue.f16.positive.pi.half,
        third: kValue.f16.positive.pi.third,
        quarter: kValue.f16.positive.pi.quarter,
        sixth: kValue.f16.positive.pi.sixth,
      },
      e: kValue.f16.positive.e,
    },
    negative: {
      min: kValue.f16.negative.min,
      max: kValue.f16.negative.max,
      infinity: kValue.f16.infinity.negative,
      nearest_min: kValue.f16.negative.nearest_min,
      less_than_one: kValue.f16.negative.less_than_one,
      subnormal: {
        min: kValue.f16.subnormal.negative.min,
        max: kValue.f16.subnormal.negative.max,
      },
      pi: {
        whole: kValue.f16.negative.pi.whole,
        three_quarters: kValue.f16.negative.pi.three_quarters,
        half: kValue.f16.negative.pi.half,
        third: kValue.f16.negative.pi.third,
        quarter: kValue.f16.negative.pi.quarter,
        sixth: kValue.f16.negative.pi.sixth,
      },
    },
    unboundedInterval: kF16UnboundedInterval,
    zeroInterval: kF16ZeroInterval,
    // Have to use the constants.ts values here, because values defined in the
    // initializer cannot be referenced in the initializer
    negPiToPiInterval: new FPInterval(
      'f16',
      kValue.f16.negative.pi.whole,
      kValue.f16.positive.pi.whole
    ),
    greaterThanZeroInterval: new FPInterval(
      'f16',
      kValue.f16.subnormal.positive.min,
      kValue.f16.positive.max
    ),
    zeroVector: {
      2: [kF16ZeroInterval, kF16ZeroInterval],
      3: [kF16ZeroInterval, kF16ZeroInterval, kF16ZeroInterval],
      4: [kF16ZeroInterval, kF16ZeroInterval, kF16ZeroInterval, kF16ZeroInterval],
    },
    unboundedVector: {
      2: [kF16UnboundedInterval, kF16UnboundedInterval],
      3: [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
      4: [
        kF16UnboundedInterval,
        kF16UnboundedInterval,
        kF16UnboundedInterval,
        kF16UnboundedInterval,
      ],
    },
    unboundedMatrix: {
      2: {
        2: [
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        3: [
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        4: [
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
        ],
      },
      3: {
        2: [
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        3: [
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        4: [
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
        ],
      },
      4: {
        2: [
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        3: [
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
          [kF16UnboundedInterval, kF16UnboundedInterval, kF16UnboundedInterval],
        ],
        4: [
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
          [
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
            kF16UnboundedInterval,
          ],
        ],
      },
    },
  };

  public constructor() {
    super('f16');
  }

  public constants(): FPConstants {
    return F16Traits._constants;
  }

  // Utilities - Overrides
  public readonly quantize = quantizeToF16;
  public readonly correctlyRounded = correctlyRoundedF16;
  public readonly isFinite = isFiniteF16;
  public readonly isSubnormal = isSubnormalNumberF16;
  public readonly flushSubnormal = flushSubnormalNumberF16;
  public readonly oneULP = oneULPF16;
  public readonly scalarBuilder = f16;

  // Framework - Fundamental Error Intervals - Overrides
  public readonly absoluteErrorInterval = this.absoluteErrorIntervalImpl.bind(this);
  public readonly correctlyRoundedInterval = this.correctlyRoundedIntervalImpl.bind(this);
  public readonly correctlyRoundedMatrix = this.correctlyRoundedMatrixImpl.bind(this);
  public readonly ulpInterval = this.ulpIntervalImpl.bind(this);

  // Framework - API - Overrides
  public readonly absInterval = this.absIntervalImpl.bind(this);
  public readonly acosInterval = this.acosIntervalImpl.bind(this);
  public readonly acoshAlternativeInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly acoshPrimaryInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly acoshIntervals = [this.acoshAlternativeInterval, this.acoshPrimaryInterval];
  public readonly additionInterval = this.additionIntervalImpl.bind(this);
  public readonly additionMatrixMatrixInterval = this.additionMatrixMatrixIntervalImpl.bind(this);
  public readonly asinInterval = this.asinIntervalImpl.bind(this);
  public readonly asinhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly atanInterval = this.atanIntervalImpl.bind(this);
  public readonly atan2Interval = this.atan2IntervalImpl.bind(this);
  public readonly atanhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly ceilInterval = this.ceilIntervalImpl.bind(this);
  public readonly clampMedianInterval = this.clampMedianIntervalImpl.bind(this);
  public readonly clampMinMaxInterval = this.clampMinMaxIntervalImpl.bind(this);
  public readonly clampIntervals = [this.clampMedianInterval, this.clampMinMaxInterval];
  public readonly cosInterval = this.cosIntervalImpl.bind(this);
  public readonly coshInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly crossInterval = this.unimplementedVectorPairToVector.bind(this);
  public readonly degreesInterval = this.degreesIntervalImpl.bind(this);
  public readonly determinantInterval = this.unimplementedMatrixToInterval.bind(this);
  public readonly distanceInterval = this.unimplementedDistance.bind(this);
  public readonly divisionInterval = this.divisionIntervalImpl.bind(this);
  public readonly dotInterval = this.dotIntervalImpl.bind(this);
  public readonly expInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly exp2Interval = this.unimplementedScalarToInterval.bind(this);
  public readonly faceForwardIntervals = this.unimplementedFaceForward.bind(this);
  public readonly floorInterval = this.floorIntervalImpl.bind(this);
  public readonly fmaInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly fractInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly inverseSqrtInterval = this.inverseSqrtIntervalImpl.bind(this);
  public readonly ldexpInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly lengthInterval = this.unimplementedLength.bind(this);
  public readonly logInterval = this.logIntervalImpl.bind(this);
  public readonly log2Interval = this.log2IntervalImpl.bind(this);
  public readonly maxInterval = this.maxIntervalImpl.bind(this);
  public readonly minInterval = this.minIntervalImpl.bind(this);
  public readonly mixImpreciseInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly mixPreciseInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly mixIntervals = [this.mixImpreciseInterval, this.mixPreciseInterval];
  public readonly modfInterval = this.unimplementedModf.bind(this);
  public readonly multiplicationInterval = this.multiplicationIntervalImpl.bind(this);
  public readonly multiplicationMatrixMatrixInterval = this.multiplicationMatrixMatrixIntervalImpl.bind(
    this
  );
  public readonly multiplicationMatrixScalarInterval = this.multiplicationMatrixScalarIntervalImpl.bind(
    this
  );
  public readonly multiplicationScalarMatrixInterval = this.multiplicationScalarMatrixIntervalImpl.bind(
    this
  );
  public readonly multiplicationMatrixVectorInterval = this.multiplicationMatrixVectorIntervalImpl.bind(
    this
  );
  public readonly multiplicationVectorMatrixInterval = this.multiplicationVectorMatrixIntervalImpl.bind(
    this
  );
  public readonly negationInterval = this.negationIntervalImpl.bind(this);
  public readonly normalizeInterval = this.unimplementedVectorToVector.bind(this);
  public readonly powInterval = this.unimplementedScalarPairToInterval.bind(this);
  public readonly quantizeToF16Interval = this.quantizeToF16IntervalNotAvailable.bind(this);
  public readonly radiansInterval = this.radiansIntervalImpl.bind(this);
  public readonly reflectInterval = this.unimplementedVectorPairToVector.bind(this);
  public readonly refractInterval = this.unimplementedRefract.bind(this);
  public readonly remainderInterval = this.remainderIntervalImpl.bind(this);
  public readonly roundInterval = this.roundIntervalImpl.bind(this);
  public readonly saturateInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly signInterval = this.signIntervalImpl.bind(this);
  public readonly sinInterval = this.sinIntervalImpl.bind(this);
  public readonly sinhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly smoothStepInterval = this.unimplementedScalarTripleToInterval.bind(this);
  public readonly sqrtInterval = this.sqrtIntervalImpl.bind(this);
  public readonly stepInterval = this.stepIntervalImpl.bind(this);
  public readonly subtractionInterval = this.subtractionIntervalImpl.bind(this);
  public readonly subtractionMatrixMatrixInterval = this.subtractionMatrixMatrixIntervalImpl.bind(
    this
  );
  public readonly tanInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly tanhInterval = this.unimplementedScalarToInterval.bind(this);
  public readonly transposeInterval = this.transposeIntervalImpl.bind(this);
  public readonly truncInterval = this.truncIntervalImpl.bind(this);

  /** quantizeToF16 has no f16 overload. */
  private quantizeToF16IntervalNotAvailable(n: number): FPInterval {
    unreachable("quantizeToF16 don't have f16 overload.");
    return kF16UnboundedInterval;
  }
}

export const FP = {
  f32: new F32Traits(),
  f16: new F16Traits(),
  abstract: new FPAbstractTraits(),
};

/** @returns the floating-point traits for @p type */
export function fpTraitsFor(type: ScalarType): FPTraits {
  switch (type.kind) {
    case 'abstract-float':
      return FP.abstract;
    case 'f32':
      return FP.f32;
    case 'f16':
      return FP.f16;
    default:
      unreachable(`unsupported type: ${type}`);
  }
}

/** @returns true if the value @p value is representable with @p type */
export function isRepresentable(value: number, type: ScalarType) {
  if (!Number.isFinite(value)) {
    return false;
  }
  if (isFloatType(type)) {
    const constants = fpTraitsFor(type).constants();
    return value >= constants.negative.min && value <= constants.positive.max;
  }
  assert(false, `isRepresentable() is not yet implemented for type ${type}`);
}
