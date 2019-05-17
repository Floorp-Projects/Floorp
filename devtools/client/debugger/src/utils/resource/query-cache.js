/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ResourceBound, ResourceState } from "./core";
import type {
  ResourceQuery,
  QueryCacheHandler,
  QueryContext,
  QueryResult,
} from "./base-query";
import { strictEqual, shallowEqual } from "./compare";

export type WeakArgsBound =
  | $ReadOnly<{ [string]: mixed }>
  | $ReadOnlyArray<mixed>;

export type ShallowArgsBound =
  | $ReadOnly<{ [string]: mixed }>
  | $ReadOnlyArray<mixed>;

/**
 * A query 'cache' function that uses the identity of the arguments object to
 * cache data for the query itself.
 */
export function queryCacheWeak<
  R: ResourceBound,
  Args: WeakArgsBound,
  Mapped,
  Reduced
>(
  handler: QueryCacheHandler<R, Args, Mapped, Reduced>
): ResourceQuery<R, Args, Reduced> {
  const cache = new WeakMap();
  return makeCacheFunction({
    handler,
    // The WeakMap will only return entries for the exact object,
    // so there is no need to compare at all.
    compareArgs: () => true,
    getEntry: args => cache.get(args) || null,
    setEntry: (args, entry) => {
      cache.set(args, entry);
    },
  });
}

/**
 * A query 'cache' function that uses shallow comparison to cache the most
 * recent calculated result based on the value of the argument.
 */
export function queryCacheShallow<
  R: ResourceBound,
  // We require args to be an object here because if you're using a primitive
  // then you should be using queryCacheStrict instead.
  Args: ShallowArgsBound,
  Mapped,
  Reduced
>(
  handler: QueryCacheHandler<R, Args, Mapped, Reduced>
): ResourceQuery<R, Args, Reduced> {
  let latestEntry = null;
  return makeCacheFunction({
    handler,
    compareArgs: shallowEqual,
    getEntry: () => latestEntry,
    setEntry: (args, entry) => {
      latestEntry = entry;
    },
  });
}

/**
 * A query 'cache' function that uses strict comparison to cache the most
 * recent calculated result based on the value of the argument.
 */
export function queryCacheStrict<R: ResourceBound, Args, Mapped, Reduced>(
  handler: QueryCacheHandler<R, Args, Mapped, Reduced>
): ResourceQuery<R, Args, Reduced> {
  let latestEntry = null;
  return makeCacheFunction({
    handler,
    compareArgs: strictEqual,
    getEntry: () => latestEntry,
    setEntry: (args, entry) => {
      latestEntry = entry;
    },
  });
}

type CacheEntry<R: ResourceBound, Args, Mapped, Reduced> = {
  context: QueryContext<Args>,
  state: ResourceState<R>,
  result: QueryResult<Mapped, Reduced>,
};

type CacheFunctionInfo<R: ResourceBound, Args, Mapped, Reduced> = {|
  // The handler to call when the args or the state are different from
  // those in the entry for the arguments.
  handler: QueryCacheHandler<R, Args, Mapped, Reduced>,

  // Compare two sets of arguments to decide whether or not they should be
  // treated as the same set of arguments from the standpoint of caching.
  compareArgs: (a: Args, b: Args) => boolean,

  getEntry: (args: Args) => CacheEntry<R, Args, Mapped, Reduced> | null,
  setEntry: (args: Args, entry: CacheEntry<R, Args, Mapped, Reduced>) => void,
|};
function makeCacheFunction<R: ResourceBound, Args, Mapped, Reduced>(
  info: CacheFunctionInfo<R, Args, Mapped, Reduced>
): ResourceQuery<R, Args, Reduced> {
  const { handler, compareArgs, getEntry, setEntry } = info;

  return (state, args: Args) => {
    let entry = getEntry(args);

    const sameArgs = !!entry && compareArgs(entry.context.args, args);
    const sameState = !!entry && entry.state === state;

    if (!entry || !sameArgs || !sameState) {
      const context =
        !entry || !sameArgs
          ? {
              args,
              identMap: new WeakMap(),
            }
          : entry.context;

      const result = handler(state, context, entry ? entry.result : null);

      if (entry) {
        entry.context = context;
        entry.state = state;
        entry.result = result;
      } else {
        entry = {
          context,
          state,
          result,
        };
        setEntry(args, entry);
      }
    }

    return entry.result.reduced;
  };
}
