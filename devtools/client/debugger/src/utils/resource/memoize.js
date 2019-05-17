/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ResourceBound } from "./core";
import type { QueryMap } from "./base-query";
import { shallowEqual } from "./compare";

/**
 * Wraps a 'mapper' function to create a shallow-equality memoized version
 * of the mapped result. The returned function will return the same value
 * even if the input object is different, as long as the identity is the same
 * and the mapped result is shallow-equal to the most recent mapped value.
 */
export function memoizeResourceShallow<
  R: ResourceBound,
  Args,
  Mapped,
  T: QueryMap<R, Args, Mapped>
>(map: T): T {
  const cache = new WeakMap();

  const fn = (input, identity, args) => {
    let existingEntry = cache.get(identity);

    if (!existingEntry || existingEntry.input !== input) {
      const mapper = (map: any);
      const output = mapper(input, identity, args);

      if (existingEntry) {
        // If the new output is shallow-equal to the old output, we reuse
        // the previous object instead to preserve object equality.
        const newOutput = shallowEqual(output, existingEntry.output)
          ? existingEntry.output
          : output;

        existingEntry.output = newOutput;
        existingEntry.input = input;
      } else {
        existingEntry = {
          input,
          output,
        };
        cache.set(identity, existingEntry);
      }
    }

    return existingEntry.output;
  };
  fn.needsArgs = map.needsArgs;
  return (fn: any);
}
