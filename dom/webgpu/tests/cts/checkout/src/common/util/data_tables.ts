import { ResolveType, ZipKeysWithValues } from './types.js';

export type valueof<K> = K[keyof K];

export function keysOf<T extends string>(obj: { [k in T]: unknown }): readonly T[] {
  return (Object.keys(obj) as unknown[]) as T[];
}

export function numericKeysOf<T>(obj: object): readonly T[] {
  return (Object.keys(obj).map(n => Number(n)) as unknown[]) as T[];
}

/**
 * @returns a new Record from @p objects, using the string returned by Object.toString() as the keys
 * and the objects as the values.
 */
export function objectsToRecord<T extends Object>(objects: readonly T[]): Record<string, T> {
  const record = {};
  return objects.reduce((obj, type) => {
    return {
      ...obj,
      [type.toString()]: type,
    };
  }, record);
}

/**
 * Creates an info lookup object from a more nicely-formatted table. See below for examples.
 *
 * Note: Using `as const` on the arguments to this function is necessary to infer the correct type.
 */
export function makeTable<
  Members extends readonly string[],
  Defaults extends readonly unknown[],
  Table extends { readonly [k: string]: readonly unknown[] }
>(
  members: Members,
  defaults: Defaults,
  table: Table
): {
  readonly [k in keyof Table]: ResolveType<ZipKeysWithValues<Members, Table[k], Defaults>>;
} {
  const result: { [k: string]: { [m: string]: unknown } } = {};
  for (const [k, v] of Object.entries<readonly unknown[]>(table)) {
    const item: { [m: string]: unknown } = {};
    for (let i = 0; i < members.length; ++i) {
      item[members[i]] = v[i] ?? defaults[i];
    }
    result[k] = item;
  }
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  return result as any;
}
