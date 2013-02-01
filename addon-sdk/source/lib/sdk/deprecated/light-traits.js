/* vim:ts=2:sts=2:sw=2:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

// `var` is being used in the module in order to make it reusable in
// environments in which `let` is not yet supported.

// Shortcut to `Object.prototype.hasOwnProperty.call`.
// owns(object, name) would be the same as
// Object.prototype.hasOwnProperty.call(object, name);
var owns = Function.prototype.call.bind(Object.prototype.hasOwnProperty);

/**
 * Whether or not given property descriptors are equivalent. They are
 * equivalent either if both are marked as 'conflict' or 'required' property
 * or if all the properties of descriptors are equal.
 * @param {Object} actual
 * @param {Object} expected
 */
function equivalentDescriptors(actual, expected) {
  return (actual.conflict && expected.conflict) ||
         (actual.required && expected.required) ||
         equalDescriptors(actual, expected);
}
/**
 * Whether or not given property descriptors define equal properties.
 */
function equalDescriptors(actual, expected) {
  return actual.get === expected.get &&
         actual.set === expected.set &&
         actual.value === expected.value &&
         !!actual.enumerable === !!expected.enumerable &&
         !!actual.configurable === !!expected.configurable &&
         !!actual.writable === !!expected.writable;
}

// Utilities that throwing exceptions for a properties that are marked
// as "required" or "conflict" properties.
function throwConflictPropertyError(name) {
  throw new Error("Remaining conflicting property: `" + name + "`");
}
function throwRequiredPropertyError(name) {
  throw new Error("Missing required property: `" + name + "`");
}

/**
 * Generates custom **required** property descriptor. Descriptor contains
 * non-standard property `required` that is equal to `true`.
 * @param {String} name
 *    property name to generate descriptor for.
 * @returns {Object}
 *    custom property descriptor
 */
function RequiredPropertyDescriptor(name) {
  // Creating function by binding first argument to a property `name` on the
  // `throwConflictPropertyError` function. Created function is used as a
  // getter & setter of the created property descriptor. This way we ensure
  // that we throw exception late (on property access) if object with
  // `required` property was instantiated using built-in `Object.create`.
  var accessor = throwRequiredPropertyError.bind(null, name);
  return { get: accessor, set: accessor, required: true };
}

/**
 * Generates custom **conflicting** property descriptor. Descriptor contains
 * non-standard property `conflict` that is equal to `true`.
 * @param {String} name
 *    property name to generate descriptor for.
 * @returns {Object}
 *    custom property descriptor
 */
function ConflictPropertyDescriptor(name) {
  // For details see `RequiredPropertyDescriptor` since idea is same.
  var accessor = throwConflictPropertyError.bind(null, name);
  return { get: accessor, set: accessor, conflict: true };
}

/**
 * Tests if property is marked as `required` property.
 */
function isRequiredProperty(object, name) {
  return !!object[name].required;
}

/**
 * Tests if property is marked as `conflict` property.
 */
function isConflictProperty(object, name) {
  return !!object[name].conflict;
}

/**
 * Function tests whether or not method of the `source` object with a given
 * `name` is inherited from `Object.prototype`.
 */
function isBuiltInMethod(name, source) {
  var target = Object.prototype[name];

  // If methods are equal then we know it's `true`.
  return target == source ||
  // If `source` object comes form a different sandbox `==` will evaluate
  // to `false`, in that case we check if functions names and sources match.
         (String(target) === String(source) && target.name === source.name);
}

/**
 * Function overrides `toString` and `constructor` methods of a given `target`
 * object with a same-named methods of a given `source` if methods of `target`
 * object are inherited / copied from `Object.prototype`.
 * @see create
 */
function overrideBuiltInMethods(target, source) {
  if (isBuiltInMethod("toString", target.toString)) {
    Object.defineProperty(target, "toString",  {
      value: source.toString,
      configurable: true,
      enumerable: false
    });
  }

  if (isBuiltInMethod("constructor", target.constructor)) {
    Object.defineProperty(target, "constructor", {
      value: source.constructor,
      configurable: true,
      enumerable: false
    });
  }
}

/**
 * Composes new trait with the same own properties as the original trait,
 * except that all property names appearing in the first argument are replaced
 * by "required" property descriptors.
 * @param {String[]} keys
 *    Array of strings property names.
 * @param {Object} trait
 *    A trait some properties of which should be excluded.
 * @returns {Object}
 * @example
 *    var newTrait = exclude(["name", ...], trait)
 */
function exclude(names, trait) {
  var map = {};

  Object.keys(trait).forEach(function(name) {

    // If property is not excluded (the array of names does not contain it),
    // or it is a "required" property, copy it to the property descriptor `map`
    // that will be used for creation of resulting trait.
    if (!~names.indexOf(name) || isRequiredProperty(trait, name))
      map[name] = { value: trait[name], enumerable: true };

    // For all the `names` in the exclude name array we create required
    // property descriptors and copy them to the `map`.
    else
      map[name] = { value: RequiredPropertyDescriptor(name), enumerable: true };
  });

  return Object.create(Trait.prototype, map);
}

/**
 * Composes new instance of `Trait` with a properties of a given `trait`,
 * except that all properties whose name is an own property of `renames` will
 * be renamed to `renames[name]` and a `"required"` property for name will be
 * added instead.
 *
 * For each renamed property, a required property is generated. If
 * the `renames` map two properties to the same name, a conflict is generated.
 * If the `renames` map a property to an existing unrenamed property, a
 * conflict is generated.
 *
 * @param {Object} renames
 *    An object whose own properties serve as a mapping from old names to new
 *    names.
 * @param {Object} trait
 *    A new trait with renamed properties.
 * @returns {Object}
 * @example
 *
 *    // Return trait with `bar` property equal to `trait.foo` and with
 *    // `foo` and `baz` "required" properties.
 *    var renamedTrait = rename({ foo: "bar", baz: null }), trait);
 *
 *    // t1 and t2 are equivalent traits
 *    var t1 = rename({a: "b"}, t);
 *    var t2 = compose(exclude(["a"], t), { a: { required: true }, b: t[a] });
 */
function rename(renames, trait) {
  var map = {};

  // Loop over all the properties of the given `trait` and copy them to a
  // property descriptor `map` that will be used for the creation of the
  // resulting trait.  Also, rename properties in the `map` as specified by
  // `renames`.
  Object.keys(trait).forEach(function(name) {
    var alias;

    // If the property is in the `renames` map, and it isn't a "required"
    // property (which should never need to be aliased because "required"
    // properties never conflict), then we must try to rename it.
    if (owns(renames, name) && !isRequiredProperty(trait, name)) {
      alias = renames[name];

      // If the `map` already has the `alias`, and it isn't a "required"
      // property, that means the `alias` conflicts with an existing name for a
      // provided trait (that can happen if >=2 properties are aliased to the
      // same name). In this case we mark it as a conflicting property.
      // Otherwise, everything is fine, and we copy property with an `alias`
      // name.
      if (owns(map, alias) && !map[alias].value.required) {
        map[alias] = {
          value: ConflictPropertyDescriptor(alias),
          enumerable: true
        };
      }
      else {
        map[alias] = {
          value: trait[name],
          enumerable: true
        };
      }

      // Regardless of whether or not the rename was successful, we check to
      // see if the original `name` exists in the map (such a property
      // could exist if previous another property was aliased to this `name`).
      // If it isn't, we mark it as "required", to make sure the caller
      // provides another value for the old name, which methods of the trait
      // might continue to reference.
      if (!owns(map, name)) {
        map[name] = {
          value: RequiredPropertyDescriptor(name),
          enumerable: true
        };
      }
    }

    // Otherwise, either the property isn't in the `renames` map (thus the
    // caller is not trying to rename it) or it is a "required" property.
    // Either way, we don't have to alias the property, we just have to copy it
    // to the map.
    else {
      // The property isn't in the map yet, so we copy it over.
      if (!owns(map, name)) {
        map[name] = { value: trait[name], enumerable: true };
      }

      // The property is already in the map (that means another property was
      // aliased with this `name`, which creates a conflict if the property is
      // not marked as "required"), so we have to mark it as a "conflict"
      // property.
      else if (!isRequiredProperty(trait, name)) {
        map[name] = {
          value: ConflictPropertyDescriptor(name),
          enumerable: true
        };
      }
    }
  });
  return Object.create(Trait.prototype, map);
}

/**
 * Composes new resolved trait, with all the same properties as the original
 * `trait`, except that all properties whose name is an own property of
 * `resolutions` will be renamed to `resolutions[name]`.
 *
 * If `resolutions[name]` is `null`, the value is mapped to a property
 * descriptor that is marked as a "required" property.
 */
function resolve(resolutions, trait) {
    var renames = {};
    var exclusions = [];

    // Go through each mapping in `resolutions` object and distribute it either
    // to `renames` or `exclusions`.
    Object.keys(resolutions).forEach(function(name) {

      // If `resolutions[name]` is a truthy value then it's a mapping old -> new
      // so we copy it to `renames` map.
      if (resolutions[name])
        renames[name] = resolutions[name];

      // Otherwise it's not a mapping but an exclusion instead in which case we
      // add it to the `exclusions` array.
      else
        exclusions.push(name);
    });

    // First `exclude` **then** `rename` and order is important since
    // `exclude` and `rename` are not associative.
    return rename(renames, exclude(exclusions, trait));
}

/**
 * Create a Trait (a custom property descriptor map) that represents the given
 * `object`'s own properties. Property descriptor map is a "custom", because it
 * inherits from `Trait.prototype` and it's property descriptors may contain
 * two attributes that is not part of the ES5 specification:
 *
 *  - "required" (this property must be provided by another trait
 *    before an instance of this trait can be created)
 *  - "conflict" (when the trait is composed with another trait,
 *    a unique value for this property is provided by two or more traits)
 *
 * Data properties bound to the `Trait.required` singleton exported by
 * this module will be marked as "required" properties.
 *
 * @param {Object} object
 *    Map of properties to compose trait from.
 * @returns {Trait}
 *    Trait / Property descriptor map containing all the own properties of the
 *    given argument.
 */
function trait(object) {
  var map;
  var trait = object;

  if (!(object instanceof Trait)) {
    // If the passed `object` is not already an instance of `Trait`, we create
    // a property descriptor `map` containing descriptors for the own properties
    // of the given `object`.  `map` is then used to create a `Trait` instance
    // after all properties are mapped.  Note that we can't create a trait and
    // then just copy properties into it since that will fail for inherited
    // read-only properties.
    map = {};

    // Each own property of the given `object` is mapped to a data property
    // whose value is a property descriptor.
    Object.keys(object).forEach(function (name) {

      // If property of an `object` is equal to a `Trait.required`, it means
      // that it was marked as "required" property, in which case we map it
      // to "required" property.
      if (Trait.required ==
          Object.getOwnPropertyDescriptor(object, name).value) {
        map[name] = {
          value: RequiredPropertyDescriptor(name),
          enumerable: true
        };
      }
      // Otherwise property is mapped to it's property descriptor.
      else {
        map[name] = {
          value: Object.getOwnPropertyDescriptor(object, name),
          enumerable: true
        };
      }
    });

    trait = Object.create(Trait.prototype, map);
  }
  return trait;
}

/**
 * Compose a property descriptor map that inherits from `Trait.prototype` and
 * contains property descriptors for all the own properties of the passed
 * traits.
 *
 * If two or more traits have own properties with the same name, the returned
 * trait will contain a "conflict" property for that name. Composition is a
 * commutative and associative operation, and the order of its arguments is
 * irrelevant.
 */
function compose(trait1, trait2/*, ...*/) {
  // Create a new property descriptor `map` to which all the own properties
  // of the passed traits are copied.  This map will be used to create a `Trait`
  // instance that will be the result of this composition.
  var map = {};

  // Properties of each passed trait are copied to the composition.
  Array.prototype.forEach.call(arguments, function(trait) {
    // Copying each property of the given trait.
    Object.keys(trait).forEach(function(name) {

      // If `map` already owns a property with the `name` and it is not
      // marked "required".
      if (owns(map, name) && !map[name].value.required) {

        // If the source trait's property with the `name` is marked as
        // "required", we do nothing, as the requirement was already resolved
        // by a property in the `map` (because it already contains a
        // non-required property with that `name`).  But if properties are just
        // different, we have a name clash and we substitute it with a property
        // that is marked "conflict".
        if (!isRequiredProperty(trait, name) &&
            !equivalentDescriptors(map[name].value, trait[name])
        ) {
          map[name] = {
            value: ConflictPropertyDescriptor(name),
            enumerable: true
          };
        }
      }

      // Otherwise, the `map` does not have an own property with the `name`, or
      // it is marked "required".  Either way, the trait's property is copied to
      // the map (if the property of the `map` is marked "required", it is going
      // to be resolved by the property that is being copied).
      else {
        map[name] = { value: trait[name], enumerable: true };
      }
    });
  });

  return Object.create(Trait.prototype, map);
}

/**
 *  `defineProperties` is like `Object.defineProperties`, except that it
 *  ensures that:
 *    - An exception is thrown if any property in a given `properties` map
 *      is marked as "required" property and same named property is not
 *      found in a given `prototype`.
 *    - An exception is thrown if any property in a given `properties` map
 *      is marked as "conflict" property.
 * @param {Object} object
 *    Object to define properties on.
 * @param {Object} properties
 *    Properties descriptor map.
 * @returns {Object}
 *    `object` that was passed as a first argument.
 */
function defineProperties(object, properties) {

  // Create a map into which we will copy each verified property from the given
  // `properties` description map. We use it to verify that none of the
  // provided properties is marked as a "conflict" property and that all
  // "required" properties are resolved by a property of an `object`, so we
  // can throw an exception before mutating object if that isn't the case.
  var verifiedProperties = {};

  // Coping each property from a given `properties` descriptor map to a
  // verified map of property descriptors.
  Object.keys(properties).forEach(function(name) {

    // If property is marked as "required" property and we don't have a same
    // named property in a given `object` we throw an exception. If `object`
    // has same named property just skip this property since required property
    // is was inherited and there for requirement was satisfied.
    if (isRequiredProperty(properties, name)) {
      if (!(name in object))
        throwRequiredPropertyError(name);
    }

    // If property is marked as "conflict" property we throw an exception.
    else if (isConflictProperty(properties, name)) {
      throwConflictPropertyError(name);
    }

    // If property is not marked neither as "required" nor "conflict" property
    // we copy it to verified properties map.
    else {
      verifiedProperties[name] = properties[name];
    }
  });

  // If no exceptions were thrown yet, we know that our verified property
  // descriptor map has no properties marked as "conflict" or "required",
  // so we just delegate to the built-in `Object.defineProperties`.
  return Object.defineProperties(object, verifiedProperties);
}

/**
 *  `create` is like `Object.create`, except that it ensures that:
 *    - An exception is thrown if any property in a given `properties` map
 *      is marked as "required" property and same named property is not
 *      found in a given `prototype`.
 *    - An exception is thrown if any property in a given `properties` map
 *      is marked as "conflict" property.
 * @param {Object} prototype
 *    prototype of the composed object
 * @param {Object} properties
 *    Properties descriptor map.
 * @returns {Object}
 *    An object that inherits form a given `prototype` and implements all the
 *    properties defined by a given `properties` descriptor map.
 */
function create(prototype, properties) {

  // Creating an instance of the given `prototype`.
  var object = Object.create(prototype);

  // Overriding `toString`, `constructor` methods if they are just inherited
  // from `Object.prototype` with a same named methods of the `Trait.prototype`
  // that will have more relevant behavior.
  overrideBuiltInMethods(object, Trait.prototype);

  // Trying to define given `properties` on the `object`. We use our custom
  // `defineProperties` function instead of build-in `Object.defineProperties`
  // that behaves exactly the same, except that it will throw if any
  // property in the given `properties` descriptor is marked as "required" or
  // "conflict" property.
  return defineProperties(object, properties);
}

/**
 * Composes new trait. If two or more traits have own properties with the
 * same name, the new trait will contain a "conflict" property for that name.
 * "compose" is a commutative and associative operation, and the order of its
 * arguments is not significant.
 *
 * **Note:** Use `Trait.compose` instead of calling this function with more
 * than one argument. The multiple-argument functionality is strictly for
 * backward compatibility.
 *
 * @params {Object} trait
 *    Takes traits as an arguments
 * @returns {Object}
 *    New trait containing the combined own properties of all the traits.
 * @example
 *    var newTrait = compose(trait_1, trait_2, ..., trait_N)
 */
function Trait(trait1, trait2) {

  // If the function was called with one argument, the argument should be
  // an object whose properties are mapped to property descriptors on a new
  // instance of Trait, so we delegate to the trait function.
  // If the function was called with more than one argument, those arguments
  // should be instances of Trait or plain property descriptor maps
  // whose properties should be mixed into a new instance of Trait,
  // so we delegate to the compose function.

  return trait2 === undefined ? trait(trait1) : compose.apply(null, arguments);
}

Object.freeze(Object.defineProperties(Trait.prototype, {
  toString: {
    value: function toString() {
      return "[object " + this.constructor.name + "]";
    }
  },

  /**
   * `create` is like `Object.create`, except that it ensures that:
   *    - An exception is thrown if this trait defines a property that is
   *      marked as required property and same named property is not
   *      found in a given `prototype`.
   *    - An exception is thrown if this trait contains property that is
   *      marked as "conflict" property.
   * @param {Object}
   *    prototype of the compared object
   * @returns {Object}
   *    An object with all of the properties described by the trait.
   */
  create: {
    value: function createTrait(prototype) {
      return create(undefined === prototype ? Object.prototype : prototype,
                    this);
    },
    enumerable: true
  },

  /**
   * Composes a new resolved trait, with all the same properties as the original
   * trait, except that all properties whose name is an own property of
   * `resolutions` will be renamed to the value of `resolutions[name]`. If
   * `resolutions[name]` is `null`, the property is marked as "required".
   * @param {Object} resolutions
   *   An object whose own properties serve as a mapping from old names to new
   *   names, or to `null` if the property should be excluded.
   * @returns {Object}
   *   New trait with the same own properties as the original trait but renamed.
   */
  resolve: {
    value: function resolveTrait(resolutions) {
      return resolve(resolutions, this);
    },
    enumerable: true
  }
}));

/**
 * @see compose
 */
Trait.compose = Object.freeze(compose);
Object.freeze(compose.prototype);

/**
 * Constant singleton, representing placeholder for required properties.
 * @type {Object}
 */
Trait.required = Object.freeze(Object.create(Object.prototype, {
  toString: {
    value: Object.freeze(function toString() {
      return "<Trait.required>";
    })
  }
}));
Object.freeze(Trait.required.toString.prototype);

exports.Trait = Object.freeze(Trait);
