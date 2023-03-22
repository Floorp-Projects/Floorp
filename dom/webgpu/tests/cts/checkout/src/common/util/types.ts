/** Forces a type to resolve its type definitions, to make it readable/debuggable. */
export type ResolveType<T> = T extends object
  ? T extends infer O
    ? { [K in keyof O]: ResolveType<O[K]> }
    : never
  : T;

/** Returns the type `true` iff X and Y are exactly equal */
export type TypeEqual<X, Y> = (<T>() => T extends X ? 1 : 2) extends <T>() => T extends Y ? 1 : 2
  ? true
  : false;

/* eslint-disable-next-line @typescript-eslint/no-unused-vars */
export function assertTypeTrue<T extends true>() {}

/**
 * Computes the intersection of a set of types, given the union of those types.
 *
 * From: https://stackoverflow.com/a/56375136
 */
export type UnionToIntersection<U> =
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  (U extends any ? (k: U) => void : never) extends (k: infer I) => void ? I : never;

/** "Type asserts" that `X` is a subtype of `Y`. */
type EnsureSubtype<X, Y> = X extends Y ? X : never;

type TupleHeadOr<T, Default> = T extends readonly [infer H, ...(readonly unknown[])] ? H : Default;
type TupleTailOr<T, Default> = T extends readonly [unknown, ...infer Tail] ? Tail : Default;
type TypeOr<T, Default> = T extends undefined ? Default : T;

/**
 * Zips a key tuple type and a value tuple type together into an object.
 *
 * @template Keys Keys of the resulting object.
 * @template Values Values of the resulting object. If a key corresponds to a `Values` member that
 *   is undefined or past the end, it defaults to the corresponding `Defaults` member.
 * @template Defaults Default values. If a key corresponds to a `Defaults` member that is past the
 *   end, the default falls back to `undefined`.
 */
export type ZipKeysWithValues<
  Keys extends readonly string[],
  Values extends readonly unknown[],
  Defaults extends readonly unknown[]
> =
  //
  Keys extends readonly [infer KHead, ...infer KTail]
    ? {
        readonly [k in EnsureSubtype<KHead, string>]: TypeOr<
          TupleHeadOr<Values, undefined>,
          TupleHeadOr<Defaults, undefined>
        >;
      } &
        ZipKeysWithValues<
          EnsureSubtype<KTail, readonly string[]>,
          TupleTailOr<Values, []>,
          TupleTailOr<Defaults, []>
        >
    : {}; // K exhausted
