/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

const {
  compose: _compose,
  override: _override,
  resolve: _resolve,
  trait: _trait,
  //create: _create,
  required,
} = require('./traits/core');

const defineProperties = Object.defineProperties,
      freeze = Object.freeze,
      create = Object.create;

/**
 * Work around bug 608959 by defining the _create function here instead of
 * importing it from traits/core.  For docs on this function, see the create
 * function in that module.
 *
 * FIXME: remove this workaround in favor of importing the function once that
 * bug has been fixed.
 */
function _create(proto, trait) {
  let properties = {},
      keys = Object.getOwnPropertyNames(trait);
  for (let key of keys) {
    let descriptor = trait[key];
    if (descriptor.required &&
        !Object.prototype.hasOwnProperty.call(proto, key))
      throw new Error('Missing required property: ' + key);
    else if (descriptor.conflict)
      throw new Error('Remaining conflicting property: ' + key);
    else
      properties[key] = descriptor;
  }
  return Object.create(proto, properties);
}

/**
 * Placeholder for `Trait.prototype`
 */
let TraitProto = Object.prototype;

function Get(key) this[key]
function Set(key, value) this[key] = value

/**
 * Creates anonymous trait descriptor from the passed argument, unless argument
 * is a trait constructor. In later case trait's already existing properties
 * descriptor is returned.
 * This is module's internal function and is used as a gateway to a trait's
 * internal properties descriptor.
 * @param {Function} $
 *    Composed trait's constructor.
 * @returns {Object}
 *    Private trait property of the composition.
 */
function TraitDescriptor(object)
  (
    'function' == typeof object &&
    (object.prototype == TraitProto || object.prototype instanceof Trait)
  ) ? object._trait(TraitDescriptor) : _trait(object)

function Public(instance, trait) {
  let result = {},
      keys = Object.getOwnPropertyNames(trait);
  for (let key of keys) {
    if ('_' === key.charAt(0) && '__iterator__' !== key )
      continue;
    let property = trait[key],
        descriptor = {
          configurable: property.configurable,
          enumerable: property.enumerable
        };
    if (property.get)
      descriptor.get = property.get.bind(instance);
    if (property.set)
      descriptor.set = property.set.bind(instance);
    if ('value' in property) {
      let value = property.value;
      if ('function' === typeof value) {
        descriptor.value = property.value.bind(instance);
        descriptor.writable = property.writable;
      } else {
        descriptor.get = Get.bind(instance, key);
        descriptor.set = Set.bind(instance, key);
      }
    }
    result[key] = descriptor;
  }
  return result;
}

/**
 * This is private function that composes new trait with privates.
 */
function Composition(trait) {
  function Trait() {
    let self = _create({}, trait);
    self._public = create(Trait.prototype, Public(self, trait));
    delete self._public.constructor;
    if (Object === self.constructor)
      self.constructor = Trait;
    else
      return self.constructor.apply(self, arguments) || self._public;
    return self._public;
  }
  defineProperties(Trait, {
    prototype: { value: freeze(create(TraitProto, {
      constructor: { value: constructor, writable: true }
    }))}, // writable is `true` to avoid getters in custom ES5
    displayName: { value: (trait.constructor || constructor).name },
    compose: { value: compose, enumerable: true },
    override: { value: override, enumerable: true },
    resolve: { value: resolve, enumerable: true },
    required: { value: required, enumerable: true },
    _trait: { value: function _trait(caller)
        caller ===  TraitDescriptor ? trait : undefined
    }
  });
  return freeze(Trait);
}

/**
 * Composes new trait out of itself and traits / property maps passed as an
 * arguments. If two or more traits / property maps have properties with the
 * same name, the new trait will contain a "conflict" property for that name.
 * This is a commutative and associative operation, and the order of its
 * arguments is not significant.
 * @params {Object|Function}
 *    List of Traits or property maps to create traits from.
 * @returns {Function}
 *    New trait containing the combined properties of all the traits.
 */
function compose() {
  let traits = Array.slice(arguments, 0);
  traits.push(this);
  return Composition(_compose.apply(null, traits.map(TraitDescriptor)));
}

/**
 * Composes a new trait with all of the combined properties of `this` and the
 * argument traits. In contrast to `compose`, `override` immediately resolves
 * all conflicts resulting from this composition by overriding the properties of
 * later traits. Trait priority is from left to right. I.e. the properties of
 * the leftmost trait are never overridden.
 * @params {Object} trait
 * @returns {Object}
 */
function override() {
  let traits = Array.slice(arguments, 0);
  traits.push(this);
  return Composition(_override.apply(null, traits.map(TraitDescriptor)));
}

/**
 * Composes new resolved trait, with all the same properties as this
 * trait, except that all properties whose name is an own property of
 * `resolutions` will be renamed to `resolutions[name]`. If it is
 * `resolutions[name]` is `null` value is changed into a required property
 * descriptor.
 */
function resolve(resolutions)
  Composition(_resolve(resolutions, TraitDescriptor(this)))

/**
 * Base Trait, that all the traits are composed of.
 */
const Trait = Composition({
  /**
   * Internal property holding public API of this instance.
   */
  _public: { value: null, configurable: true, writable: true },
  toString: { value: function() '[object ' + this.constructor.name + ']' }
});
TraitProto = Trait.prototype;
exports.Trait = Trait;

