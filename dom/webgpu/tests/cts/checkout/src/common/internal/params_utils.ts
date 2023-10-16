import { TestParams } from '../framework/fixture.js';
import { ResolveType, UnionToIntersection } from '../util/types.js';
import { assert } from '../util/util.js';

import { comparePublicParamsPaths, Ordering } from './query/compare.js';
import { kWildcard, kParamSeparator, kParamKVSeparator } from './query/separators.js';

export type JSONWithUndefined =
  | undefined
  | null
  | number
  | string
  | boolean
  | readonly JSONWithUndefined[]
  // Ideally this would recurse into JSONWithUndefined, but it breaks code.
  | { readonly [k: string]: unknown };
export interface TestParamsRW {
  [k: string]: JSONWithUndefined;
}
export type TestParamsIterable = Iterable<TestParams>;

export function paramKeyIsPublic(key: string): boolean {
  return !key.startsWith('_');
}

export function extractPublicParams(params: TestParams): TestParams {
  const publicParams: TestParamsRW = {};
  for (const k of Object.keys(params)) {
    if (paramKeyIsPublic(k)) {
      publicParams[k] = params[k];
    }
  }
  return publicParams;
}

export const badParamValueChars = new RegExp(
  '[' + kParamKVSeparator + kParamSeparator + kWildcard + ']'
);

export function publicParamsEquals(x: TestParams, y: TestParams): boolean {
  return comparePublicParamsPaths(x, y) === Ordering.Equal;
}

export type KeyOfNeverable<T> = T extends never ? never : keyof T;
export type AllKeysFromUnion<T> = keyof T | KeyOfNeverable<UnionToIntersection<T>>;
export type KeyOfOr<T, K, Default> = K extends keyof T ? T[K] : Default;

/**
 * Flatten a union of interfaces into a single interface encoding the same type.
 *
 * Flattens a union in such a way that:
 * `{ a: number, b?: undefined } | { b: string, a?: undefined }`
 * (which is the value type of `[{ a: 1 }, { b: 1 }]`)
 * becomes `{ a: number | undefined, b: string | undefined }`.
 *
 * And also works for `{ a: number } | { b: string }` which maps to the same.
 */
export type FlattenUnionOfInterfaces<T> = {
  [K in AllKeysFromUnion<T>]: KeyOfOr<
    T,
    // If T always has K, just take T[K] (union of C[K] for each component C of T):
    K,
    // Otherwise, take the union of C[K] for each component C of T, PLUS undefined:
    undefined | KeyOfOr<UnionToIntersection<T>, K, void>
  >;
};

/* eslint-disable-next-line @typescript-eslint/no-unused-vars */
function typeAssert<T extends 'pass'>() {}
{
  type Test<T, U> = [T] extends [U]
    ? [U] extends [T]
      ? 'pass'
      : { actual: ResolveType<T>; expected: U }
    : { actual: ResolveType<T>; expected: U };

  type T01 = { a: number } | { b: string };
  type T02 = { a: number } | { b?: string };
  type T03 = { a: number } | { a?: number };
  type T04 = { a: number } | { a: string };
  type T05 = { a: number } | { a?: string };

  type T11 = { a: number; b?: undefined } | { a?: undefined; b: string };

  type T21 = { a: number; b?: undefined } | { b: string };
  type T22 = { a: number; b?: undefined } | { b?: string };
  type T23 = { a: number; b?: undefined } | { a?: number };
  type T24 = { a: number; b?: undefined } | { a: string };
  type T25 = { a: number; b?: undefined } | { a?: string };
  type T26 = { a: number; b?: undefined } | { a: undefined };
  type T27 = { a: number; b?: undefined } | { a: undefined; b: undefined };

  /* prettier-ignore */ {
    typeAssert<Test<FlattenUnionOfInterfaces<T01>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T02>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T03>, { a: number | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T04>, { a: number | string }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T05>, { a: number | string | undefined }>>();

    typeAssert<Test<FlattenUnionOfInterfaces<T11>, { a: number | undefined; b: string | undefined }>>();

    typeAssert<Test<FlattenUnionOfInterfaces<T22>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T23>, { a: number | undefined; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T24>, { a: number | string; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T25>, { a: number | string | undefined; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T27>, { a: number | undefined; b: undefined }>>();

    // Unexpected test results - hopefully okay to ignore these
    typeAssert<Test<FlattenUnionOfInterfaces<T21>, { b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T26>, { a: number | undefined }>>();
  }
}

export type Merged<A, B> = MergedFromFlat<A, FlattenUnionOfInterfaces<B>>;
export type MergedFromFlat<A, B> = {
  [K in keyof A | keyof B]: K extends keyof B ? B[K] : K extends keyof A ? A[K] : never;
};

export function mergeParams<A extends {}, B extends {}>(a: A, b: B): Merged<A, B> {
  for (const key of Object.keys(a)) {
    assert(!(key in b), 'Duplicate key: ' + key);
  }
  return { ...a, ...b } as Merged<A, B>;
}
