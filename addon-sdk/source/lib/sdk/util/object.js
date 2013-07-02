/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

/**
 * Merges all the properties of all arguments into first argument. If two or
 * more argument objects have own properties with the same name, the property
 * is overridden, with precedence from right to left, implying, that properties
 * of the object on the left are overridden by a same named property of the
 * object on the right.
 *
 * Any argument given with "falsy" value - commonly `null` and `undefined` in
 * case of objects - are skipped.
 *
 * @examples
 *    var a = { bar: 0, a: 'a' }
 *    var b = merge(a, { foo: 'foo', bar: 1 }, { foo: 'bar', name: 'b' });
 *    b === a   // true
 *    b.a       // 'a'
 *    b.foo     // 'bar'
 *    b.bar     // 1
 *    b.name    // 'b'
 */
function merge(source) {
  let descriptor = {};
  // `Boolean` converts the first parameter to a boolean value. Any object is
  // converted to `true` where `null` and `undefined` becames `false`. Therefore
  // the `filter` method will keep only objects that are defined and not null.
  Array.slice(arguments, 1).filter(Boolean).forEach(function onEach(properties) {
    Object.getOwnPropertyNames(properties).forEach(function(name) {
      descriptor[name] = Object.getOwnPropertyDescriptor(properties, name);
    });
  });
  return Object.defineProperties(source, descriptor);
}
exports.merge = merge;

/**
 * Returns an object that inherits from the first argument and contains all the
 * properties from all following arguments.
 * `extend(source1, source2, source3)` is equivalent of
 * `merge(Object.create(source1), source2, source3)`.
 */
function extend(source) {
  let rest = Array.slice(arguments, 1);
  rest.unshift(Object.create(source));
  return merge.apply(null, rest);
}
exports.extend = extend;

function has(obj, key) obj.hasOwnProperty(key);
exports.has = has;

function each(obj, fn) {
  for (let key in obj) has(obj, key) && fn(obj[key], key, obj);
}
exports.each = each;
