/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { strictEqual, shallowEqual } from "./compare";

/**
 * A query 'cache' function that uses the identity of the arguments object to
 * cache data for the query itself.
 */
export function queryCacheWeak(handler) {
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
export function queryCacheShallow(handler) {
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
export function queryCacheStrict(handler) {
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

function makeCacheFunction(info) {
  const { handler, compareArgs, getEntry, setEntry } = info;

  return (state, args) => {
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
