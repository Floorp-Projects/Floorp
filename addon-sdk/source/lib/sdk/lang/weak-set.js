/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.metadata = {
  "stability": "experimental"
};

"use strict";

const { Cu } = require("chrome");

function makeGetterFor(Type) {
  let cache = new WeakMap();

  return {
    getFor(target) {
      if (!cache.has(target))
        cache.set(target, new Type());

      return cache.get(target);
    },
    clearFor(target) {
      return cache.delete(target)
    }
  }
}

var {getFor: getLookupFor, clearFor: clearLookupFor} = makeGetterFor(WeakMap);
var {getFor: getRefsFor, clearFor: clearRefsFor} = makeGetterFor(Set);

function add(target, value) {
  if (has(target, value))
    return;

  getLookupFor(target).set(value, true);
  getRefsFor(target).add(Cu.getWeakReference(value));
}
exports.add = add;

function remove(target, value) {
  getLookupFor(target).delete(value);
}
exports.remove = remove;

function has(target, value) {
  return getLookupFor(target).has(value);
}
exports.has = has;

function clear(target) {
  clearLookupFor(target);
  clearRefsFor(target);
}
exports.clear = clear;

function iterator(target) {
  let refs = getRefsFor(target);

  for (let ref of refs) {
    let value = ref.get();

    // If `value` is already gc'ed, it would be `null`.
    // The `has` function is using a WeakMap as lookup table, so passing `null`
    // would raise an exception because WeakMap accepts as value only non-null
    // object.
    // Plus, if `value` is already gc'ed, we do not have to take it in account
    // during the iteration, and remove it from the references.
    if (value !== null && has(target, value))
      yield value;
    else
      refs.delete(ref);
  }
}
exports.iterator = iterator;
