/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const create = Object.create;
const prototypeOf = Object.getPrototypeOf;

/**
 * Returns a new namespace, function that may can be used to access an
 * namespaced object of the argument argument. Namespaced object are associated
 * with owner objects via weak references. Namespaced objects inherit from the
 * owners ancestor namespaced object. If owner's ancestor is `null` then
 * namespaced object inherits from given `prototype`. Namespaces can be used
 * to define internal APIs that can be shared via enclosing `namespace`
 * function.
 * @examples
 *    const internals = ns();
 *    internals(object).secret = secret;
 */
function ns() {
  const map = new WeakMap();
  return function namespace(target) {
    if (!target)        // If `target` is not an object return `target` itself.
      return target;
    // If target has no namespaced object yet, create one that inherits from
    // the target prototype's namespaced object.
    if (!map.has(target))
      map.set(target, create(namespace(prototypeOf(target) || null)));

    return map.get(target);
  };
};

// `Namespace` is a e4x function in the scope, so we export the function also as
// `ns` as alias to avoid clashing.
exports.ns = ns;
exports.Namespace = ns;
