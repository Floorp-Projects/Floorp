import { Float16Array } from '../../external/petamoriken/float16/float16.js';
import { globalTestConfig } from '../framework/test_config.js';
import { Logger } from '../internal/logging/logger.js';

import { keysOf } from './data_tables.js';
import { timeout } from './timeout.js';

/**
 * Error with arbitrary `extra` data attached, for debugging.
 * The extra data is omitted if not running the test in debug mode (`?debug=1`).
 */
export class ErrorWithExtra extends Error {
  readonly extra: { [k: string]: unknown };

  /**
   * `extra` function is only called if in debug mode.
   * If an `ErrorWithExtra` is passed, its message is used and its extras are passed through.
   */
  constructor(message: string, extra: () => {});
  constructor(base: ErrorWithExtra, newExtra: () => {});
  constructor(baseOrMessage: string | ErrorWithExtra, newExtra: () => {}) {
    const message = typeof baseOrMessage === 'string' ? baseOrMessage : baseOrMessage.message;
    super(message);

    const oldExtras = baseOrMessage instanceof ErrorWithExtra ? baseOrMessage.extra : {};
    this.extra = Logger.globalDebugMode
      ? { ...oldExtras, ...newExtra() }
      : { omitted: 'pass ?debug=1' };
  }
}

/**
 * Asserts `condition` is true. Otherwise, throws an `Error` with the provided message.
 */
export function assert(condition: boolean, msg?: string | (() => string)): asserts condition {
  if (!condition) {
    throw new Error(msg && (typeof msg === 'string' ? msg : msg()));
  }
}

/** If the argument is an Error, throw it. Otherwise, pass it back. */
export function assertOK<T>(value: Error | T): T {
  if (value instanceof Error) {
    throw value;
  }
  return value;
}

/**
 * Resolves if the provided promise rejects; rejects if it does not.
 */
export async function assertReject(p: Promise<unknown>, msg?: string): Promise<void> {
  try {
    await p;
    unreachable(msg);
  } catch (ex) {
    // Assertion OK
  }
}

/**
 * Assert this code is unreachable. Unconditionally throws an `Error`.
 */
export function unreachable(msg?: string): never {
  throw new Error(msg);
}

/**
 * The `performance` interface.
 * It is available in all browsers, but it is not in scope by default in Node.
 */
const perf = typeof performance !== 'undefined' ? performance : require('perf_hooks').performance;

/**
 * Calls the appropriate `performance.now()` depending on whether running in a browser or Node.
 */
export function now(): number {
  return perf.now();
}

/**
 * Returns a promise which resolves after the specified time.
 */
export function resolveOnTimeout(ms: number): Promise<void> {
  return new Promise(resolve => {
    timeout(() => {
      resolve();
    }, ms);
  });
}

export class PromiseTimeoutError extends Error {}

/**
 * Returns a promise which rejects after the specified time.
 */
export function rejectOnTimeout(ms: number, msg: string): Promise<never> {
  return new Promise((_resolve, reject) => {
    timeout(() => {
      reject(new PromiseTimeoutError(msg));
    }, ms);
  });
}

/**
 * Takes a promise `p`, and returns a new one which rejects if `p` takes too long,
 * and otherwise passes the result through.
 */
export function raceWithRejectOnTimeout<T>(p: Promise<T>, ms: number, msg: string): Promise<T> {
  if (globalTestConfig.noRaceWithRejectOnTimeout) {
    return p;
  }
  // Setup a promise that will reject after `ms` milliseconds. We cancel this timeout when
  // `p` is finalized, so the JavaScript VM doesn't hang around waiting for the timer to
  // complete, once the test runner has finished executing the tests.
  const timeoutPromise = new Promise((_resolve, reject) => {
    const handle = timeout(() => {
      reject(new PromiseTimeoutError(msg));
    }, ms);
    p = p.finally(() => clearTimeout(handle));
  });
  return Promise.race([p, timeoutPromise]) as Promise<T>;
}

/**
 * Takes a promise `p` and returns a new one which rejects if `p` resolves or rejects,
 * and otherwise resolves after the specified time.
 */
export function assertNotSettledWithinTime(
  p: Promise<unknown>,
  ms: number,
  msg: string
): Promise<undefined> {
  // Rejects regardless of whether p resolves or rejects.
  const rejectWhenSettled = p.then(() => Promise.reject(new Error(msg)));
  // Resolves after `ms` milliseconds.
  const timeoutPromise = new Promise<undefined>(resolve => {
    const handle = timeout(() => {
      resolve(undefined);
    }, ms);
    p.finally(() => clearTimeout(handle));
  });
  return Promise.race([rejectWhenSettled, timeoutPromise]);
}

/**
 * Returns a `Promise.reject()`, but also registers a dummy `.catch()` handler so it doesn't count
 * as an uncaught promise rejection in the runtime.
 */
export function rejectWithoutUncaught<T>(err: unknown): Promise<T> {
  const p = Promise.reject(err);
  // Suppress uncaught promise rejection.
  p.catch(() => {});
  return p;
}

/**
 * Makes a copy of a JS `object`, with the keys reordered into sorted order.
 */
export function sortObjectByKey(v: { [k: string]: unknown }): { [k: string]: unknown } {
  const sortedObject: { [k: string]: unknown } = {};
  for (const k of Object.keys(v).sort()) {
    sortedObject[k] = v[k];
  }
  return sortedObject;
}

/**
 * Determines whether two JS values are equal, recursing into objects and arrays.
 * NaN is treated specially, such that `objectEquals(NaN, NaN)`.
 */
export function objectEquals(x: unknown, y: unknown): boolean {
  if (typeof x !== 'object' || typeof y !== 'object') {
    if (typeof x === 'number' && typeof y === 'number' && Number.isNaN(x) && Number.isNaN(y)) {
      return true;
    }
    return x === y;
  }
  if (x === null || y === null) return x === y;
  if (x.constructor !== y.constructor) return false;
  if (x instanceof Function) return x === y;
  if (x instanceof RegExp) return x === y;
  if (x === y || x.valueOf() === y.valueOf()) return true;
  if (Array.isArray(x) && Array.isArray(y) && x.length !== y.length) return false;
  if (x instanceof Date) return false;
  if (!(x instanceof Object)) return false;
  if (!(y instanceof Object)) return false;

  const x1 = x as { [k: string]: unknown };
  const y1 = y as { [k: string]: unknown };
  const p = Object.keys(x);
  return Object.keys(y).every(i => p.indexOf(i) !== -1) && p.every(i => objectEquals(x1[i], y1[i]));
}

/**
 * Generates a range of values `fn(0)..fn(n-1)`.
 */
export function range<T>(n: number, fn: (i: number) => T): T[] {
  return [...new Array(n)].map((_, i) => fn(i));
}

/**
 * Generates a range of values `fn(0)..fn(n-1)`.
 */
export function* iterRange<T>(n: number, fn: (i: number) => T): Iterable<T> {
  for (let i = 0; i < n; ++i) {
    yield fn(i);
  }
}

/** Creates a (reusable) iterable object that maps `f` over `xs`, lazily. */
export function mapLazy<T, R>(xs: Iterable<T>, f: (x: T) => R): Iterable<R> {
  return {
    *[Symbol.iterator]() {
      for (const x of xs) {
        yield f(x);
      }
    },
  };
}

const TypedArrayBufferViewInstances = [
  new Uint8Array(),
  new Uint8ClampedArray(),
  new Uint16Array(),
  new Uint32Array(),
  new Int8Array(),
  new Int16Array(),
  new Int32Array(),
  new Float16Array(),
  new Float32Array(),
  new Float64Array(),
] as const;

export type TypedArrayBufferView = typeof TypedArrayBufferViewInstances[number];

export type TypedArrayBufferViewConstructor<
  A extends TypedArrayBufferView = TypedArrayBufferView
> = {
  // Interface copied from Uint8Array, and made generic.
  readonly prototype: A;
  readonly BYTES_PER_ELEMENT: number;

  new (): A;
  new (elements: Iterable<number>): A;
  new (array: ArrayLike<number> | ArrayBufferLike): A;
  new (buffer: ArrayBufferLike, byteOffset?: number, length?: number): A;
  new (length: number): A;

  from(arrayLike: ArrayLike<number>): A;
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  from(arrayLike: Iterable<number>, mapfn?: (v: number, k: number) => number, thisArg?: any): A;
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  from<T>(arrayLike: ArrayLike<T>, mapfn: (v: T, k: number) => number, thisArg?: any): A;
  of(...items: number[]): A;
};

export const kTypedArrayBufferViews: {
  readonly [k: string]: TypedArrayBufferViewConstructor;
} = {
  ...(() => {
    /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
    const result: { [k: string]: any } = {};
    for (const v of TypedArrayBufferViewInstances) {
      result[v.constructor.name] = v.constructor;
    }
    return result;
  })(),
};
export const kTypedArrayBufferViewKeys = keysOf(kTypedArrayBufferViews);
export const kTypedArrayBufferViewConstructors = Object.values(kTypedArrayBufferViews);

function subarrayAsU8(
  buf: ArrayBuffer | TypedArrayBufferView,
  { start = 0, length }: { start?: number; length?: number }
): Uint8Array | Uint8ClampedArray {
  if (buf instanceof ArrayBuffer) {
    return new Uint8Array(buf, start, length);
  } else if (buf instanceof Uint8Array || buf instanceof Uint8ClampedArray) {
    // Don't wrap in new views if we don't need to.
    if (start === 0 && (length === undefined || length === buf.byteLength)) {
      return buf;
    }
  }
  const byteOffset = buf.byteOffset + start * buf.BYTES_PER_ELEMENT;
  const byteLength =
    length !== undefined
      ? length * buf.BYTES_PER_ELEMENT
      : buf.byteLength - (byteOffset - buf.byteOffset);
  return new Uint8Array(buf.buffer, byteOffset, byteLength);
}

/**
 * Copy a range of bytes from one ArrayBuffer or TypedArray to another.
 *
 * `start`/`length` are in elements (or in bytes, if ArrayBuffer).
 */
export function memcpy(
  src: { src: ArrayBuffer | TypedArrayBufferView; start?: number; length?: number },
  dst: { dst: ArrayBuffer | TypedArrayBufferView; start?: number }
): void {
  subarrayAsU8(dst.dst, dst).set(subarrayAsU8(src.src, src));
}
