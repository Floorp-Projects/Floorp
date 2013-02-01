/* vim:set ts=2 sw=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

module.metadata = {
  "stability": "unstable"
};

var getPrototypeOf = Object.getPrototypeOf;
var getNames = Object.getOwnPropertyNames;
var getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
var create = Object.create;
var freeze = Object.freeze;
var unbind = Function.call.bind(Function.bind, Function.call);

// This shortcut makes sure that we do perform desired operations, even if
// associated methods have being overridden on the used object.
var owns = unbind(Object.prototype.hasOwnProperty);
var apply = unbind(Function.prototype.apply);
var slice = Array.slice || unbind(Array.prototype.slice);
var reduce = Array.reduce || unbind(Array.prototype.reduce);
var map = Array.map || unbind(Array.prototype.map);
var concat = Array.concat || unbind(Array.prototype.concat);

// Utility function to get own properties descriptor map.
function getOwnPropertyDescriptors(object) {
  return reduce(getNames(object), function(descriptor, name) {
    descriptor[name] = getOwnPropertyDescriptor(object, name);
    return descriptor;
  }, {});
}

function isDataProperty(property) {
  var value = property.value;
  var type = typeof(property.value);
  return "value" in property &&
         (type !== "object" || value === null) &&
         type !== "function";
}

function getDataProperties(object) {
  var properties = getOwnPropertyDescriptors(object);
  return getNames(properties).reduce(function(result, name) {
    var property = properties[name];
    if (isDataProperty(property)) {
      result[name] = {
        value: property.value,
        writable: true,
        configurable: true,
        enumerable: false
      };
    }
    return result;
  }, {})
}

/**
 * Takes `source` object as an argument and returns identical object
 * with the difference that all own properties will be non-enumerable
 */
function obscure(source) {
  var descriptor = reduce(getNames(source), function(descriptor, name) {
    var property = getOwnPropertyDescriptor(source, name);
    property.enumerable = false;
    descriptor[name] = property;
    return descriptor;
  }, {});
  return create(getPrototypeOf(source), descriptor);
}
exports.obscure = obscure;

/**
 * Takes arbitrary number of source objects and returns fresh one, that
 * inherits from the same prototype as a first argument and implements all
 * own properties of all argument objects. If two or more argument objects
 * have own properties with the same name, the property is overridden, with
 * precedence from right to left, implying, that properties of the object on
 * the left are overridden by a same named property of the object on the right.
 */
var mix = function(source) {
  var descriptor = reduce(slice(arguments), function(descriptor, source) {
    return reduce(getNames(source), function(descriptor, name) {
      descriptor[name] = getOwnPropertyDescriptor(source, name);
      return descriptor;
    }, descriptor);
  }, {});

  return create(getPrototypeOf(source), descriptor);
};
exports.mix = mix;

/**
 * Returns a frozen object with that inherits from the given `prototype` and
 * implements all own properties of the given `properties` object.
 */
function extend(prototype, properties) {
  return freeze(create(prototype, getOwnPropertyDescriptors(properties)));
}
exports.extend = extend;

/**
 * Returns a constructor function with a proper `prototype` setup. Returned
 * constructor's `prototype` inherits from a given `options.extends` or
 * `Class.prototype` if omitted and implements all the properties of the
 * given `option`. If `options.implemens` array is passed, it's elements
 * will be mixed into prototype as well. Also, `options.extends` can be
 * a function or a prototype. If function than it's prototype is used as
 * an ancestor of the prototype, if it's an object that it's used directly.
 * Also `options.implements` may contain functions or objects, in case of
 * functions their prototypes are used for mixing.
 */
var Class = new function() {
  function prototypeOf(input) {
    return typeof(input) === 'function' ? input.prototype : input;
  }
  var none = freeze([]);

  return function Class(options) {
    // Create descriptor with normalized `options.extends` and
    // `options.implements`.
    var descriptor = {
      // Normalize extends property of `options.extends` to a prototype object
      // in case it's constructor. If property is missing that fallback to
      // `Type.prototype`.
      extends: owns(options, 'extends') ?
               prototypeOf(options.extends) : Class.prototype,
      // Normalize `options.implements` to make sure that it's array of
      // prototype objects instead of constructor functions.
      implements: owns(options, 'implements') ?
                  freeze(map(options.implements, prototypeOf)) : none
    };

    // Create array of property descriptors who's properties will be defined
    // on the resulting prototype. Note: Using reflection `concat` instead of
    // method as it may be overridden.
    var descriptors = concat(descriptor.implements, options, descriptor, {
      constructor: constructor
    });

    // Note: we use reflection `apply` in the constructor instead of method
    // call since later may be overridden.
    function constructor() {
      var instance = create(prototype, attributes);
      if (initialize) apply(initialize, instance, arguments);
      return instance;
    }
    // Create `prototype` that inherits from given ancestor passed as
    // `options.extends`, falling back to `Type.prototype`, implementing all
    // properties of given `options.implements` and `options` itself.
    var prototype = extend(descriptor.extends, mix.apply(mix, descriptors));
    var initialize = prototype.initialize;

    // Combine ancestor attributes with prototype's attributes so that
    // ancestors attributes also become initializeable.
    var attributes = mix(descriptor.extends.constructor.attributes || {},
                         getDataProperties(prototype));

    constructor.attributes = attributes;
    constructor.prototype = prototype;
    return freeze(constructor);
  };
}
Class.prototype = extend(null, obscure({
  constructor: function constructor() {
    this.initialize.apply(this, arguments);
    return this;
  },
  initialize: function initialize() {
    // Do your initialization logic here
  },
  // Copy useful properties from `Object.prototype`.
  toString: Object.prototype.toString,
  toLocaleString: Object.prototype.toLocaleString,
  toSource: Object.prototype.toSource,
  valueOf: Object.prototype.valueOf,
  isPrototypeOf: Object.prototype.isPrototypeOf
}));
exports.Class = freeze(Class);
