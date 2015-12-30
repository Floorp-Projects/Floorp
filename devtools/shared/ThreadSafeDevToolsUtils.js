/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * General utilities used throughout devtools that can also be used in
 * workers.
 */

/**
 * Immutably reduce the given `...objs` into one object. The reduction is
 * applied from left to right, so `immutableUpdate({ a: 1 }, { a: 2 })` will
 * result in `{ a: 2 }`. The resulting object is frozen.
 *
 * Example usage:
 *
 *     const original = { foo: 1, bar: 2, baz: 3 };
 *     const modified = immutableUpdate(original, { baz: 0, bang: 4 });
 *
 *     // We get the new object that we expect...
 *     assert(modified.baz === 0);
 *     assert(modified.bang === 4);
 *
 *     // However, the original is not modified.
 *     assert(original.baz === 2);
 *     assert(original.bang === undefined);
 *
 * @param {...Object} ...objs
 * @returns {Object}
 */
exports.immutableUpdate = function (...objs) {
  return Object.freeze(Object.assign({}, ...objs));
};
