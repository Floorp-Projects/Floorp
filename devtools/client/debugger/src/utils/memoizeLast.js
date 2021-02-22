/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function memoizeLast(fn) {
  let lastArgs;
  let lastResult;

  const memoized = (...args) => {
    if (
      lastArgs &&
      args.length === lastArgs.length &&
      args.every((arg, i) => arg === lastArgs[i])
    ) {
      return lastResult;
    }

    lastArgs = args;
    lastResult = fn(...args);

    return lastResult;
  };

  return memoized;
}

export default memoizeLast;
