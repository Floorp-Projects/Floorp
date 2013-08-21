/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

// `var` is being used in the module in order to make it reusable in
// environments in which `let` and `const` is not yet supported.

// Returns `object`'s property value, where `name` is a name of the property.
function get(object, name) {
  return object[name];
}

// Assigns `value` to the `object`'s property, where `name` is the name of the
// property.
function set(object, name, value) {
  return object[name] = value;
}

/**
 * Given an `object` containing a property with the given `name`, create
 * a property descriptor that can be used to define alias/proxy properties
 * on other objects.  A change in the value of an alias will propagate
 * to the aliased property and vice versa.
 */
function createAliasProperty(object, name) {
  // Getting own property descriptor of an `object` for the given `name` as
  // we are going to create proxy analog.
  var property = Object.getOwnPropertyDescriptor(object, name);
  var descriptor = {
    configurable: property.configurable,
    enumerable: property.enumerable,
    alias: true
  };

  // If the original property has a getter and/or setter, bind a
  // corresponding getter/setter in the alias descriptor to the original
  // object, so the `this` object in the getter/setter is the original object
  // rather than the alias.
  if ("get" in property && property.get)
    descriptor.get = property.get.bind(object);
  if ("set" in property && property.set)
    descriptor.set = property.set.bind(object);
  
  // If original property was a value property.
  if ("value" in property) {
    // If original property is a method using it's `object` bounded copy.
    if (typeof property.value === "function") {
      descriptor.value = property.value.bind(object);
      // Also preserving writability of the original property.
      descriptor.writable = property.writable;
    }

    // If the original property was just a data property, we create proxy
    // accessors using our custom get/set functions to propagate changes to the
    // original `object` and vice versa.
    else {
      descriptor.get = get.bind(null, object, name);
      descriptor.set = set.bind(null, object, name);
    }
  }
  return descriptor;
}

// Defines property on `object` object with a name `alias` if given if not
// defaults to `name` that represents an alias of `source[name]`. If aliased
// property was an assessor or a method `this` pseudo-variable will be `source`
// when invoked. If aliased property was a data property changes on any of the
// aliases will propagate to the `source[name]` and also other way round.
function defineAlias(source, target, name, alias) {
  return Object.defineProperty(target, alias || name,
                               createAliasProperty(source, name));
}

/**
 * Function takes any `object` and returns a proxy for its own public
 * properties. By default properties are considered to be public if they don't
 * start with `"_"`, but default behavior can be overridden if needed, by
 * passing array of public property `names` as a second argument. By default
 * returned object will be direct decedent of the given `object`'s prototype,
 * but this can be overridden by passing third optional argument, that will be
 * used as `prototype` instead.
 * @param {Object} object
 *    Object to create cortex for.
 * @param {String[]} [names]
 *    Optional array of public property names.
 * @param {Object} [prototype]
 *    Optional argument that will be used as `prototype` of the returned object,
 *    if not provided `Object.getPrototypeOf(object)` is used instead.
 */
exports.Cortex = function Cortex(object, names, prototype) {
  // Creating a cortex object from the given `prototype`, if one was not
  // provided then `prototype` of a given `object` is used. This allows
  // consumer to define expected behavior `instanceof`. In common case
  // `prototype` argument can be omitted to preserve same behavior of
  // `instanceof` as on original `object`.
  var cortex = Object.create(prototype || Object.getPrototypeOf(object));
  // Creating alias properties on the `cortex` object for all the own
  // properties of the original `object` that are contained in `names` array.
  // If `names` array is not provided then all the properties that don't
  // start with `"_"` are aliased.
  Object.getOwnPropertyNames(object).forEach(function (name) {
    if ((!names && "_" !== name.charAt(0)) || (names && ~names.indexOf(name)))
      defineAlias(object, cortex, name);
  });
  return cortex;
}
