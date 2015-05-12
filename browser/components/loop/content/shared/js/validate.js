/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint unused:false */

var loop = loop || {};
loop.validate = (function() {
  "use strict";

  /**
   * Computes the difference between two arrays.
   *
   * @param  {Array} arr1 First array
   * @param  {Array} arr2 Second array
   * @return {Array}      Array difference
   */
  function difference(arr1, arr2) {
    return arr1.filter(function(item) {
      return arr2.indexOf(item) === -1;
    });
  }

  /**
   * Retrieves the type name of an object or constructor. Fallback to "unknown"
   * when it fails.
   *
   * @param  {Object} obj
   * @return {String}
   */
  function typeName(obj) {
    if (obj === null) {
      return "null";
    }

    if (typeof obj === "function") {
      return obj.name || obj.toString().match(/^function\s?([^\s(]*)/)[1];
    }

    if (typeof obj.constructor === "function") {
      return typeName(obj.constructor);
    }

    return "unknown";
  }

  /**
   * Simple typed values validator.
   *
   * @constructor
   * @param  {Object} schema Validation schema
   */
  function Validator(schema) {
    this.schema = schema || {};
  }

  Validator.prototype = {
    /**
     * Validates all passed values against declared dependencies.
     *
     * @param  {Object} values  The values object
     * @return {Object}         The validated values object
     * @throws {TypeError}      If validation fails
     */
    validate: function(values) {
      this._checkRequiredProperties(values);
      this._checkRequiredTypes(values);
      return values;
    },

    /**
     * Checks if any of Object values matches any of current dependency type
     * requirements.
     *
     * @param  {Object} values The values object
     * @throws {TypeError}
     */
    _checkRequiredTypes: function(values) {
      Object.keys(this.schema).forEach(function(name) {
        var types = this.schema[name];
        types = Array.isArray(types) ? types : [types];
        if (!this._dependencyMatchTypes(values[name], types)) {
          throw new TypeError("invalid dependency: " + name +
                              "; expected " + types.map(typeName).join(", ") +
                              ", got " + typeName(values[name]));
        }
      }, this);
    },

    /**
     * Checks if a values object owns the required keys defined in dependencies.
     * Values attached to these properties shouldn't be null nor undefined.
     *
     * @param  {Object} values The values object
     * @throws {TypeError} If any dependency is missing.
     */
    _checkRequiredProperties: function(values) {
      var definedProperties = Object.keys(values).filter(function(name) {
        return typeof values[name] !== "undefined";
      });
      var diff = difference(Object.keys(this.schema), definedProperties);
      if (diff.length > 0) {
        throw new TypeError("missing required " + diff.join(", "));
      }
    },

    /**
     * Checks if a given value matches any of the provided type requirements.
     *
     * @param  {Object} value  The value to check
     * @param  {Array}  types  The list of types to check the value against
     * @return {Boolean}
     * @throws {TypeError} If the value doesn't match any types.
     */
    _dependencyMatchTypes: function(value, types) {
      return types.some(function(Type) {
        /*jshint eqeqeq:false*/
        try {
          return typeof Type === "undefined"         || // skip checking
                 Type === null && value === null     || // null type
                 value.constructor == Type           || // native type
                 Type.prototype.isPrototypeOf(value) || // custom type
                 typeName(value) === typeName(Type);    // type string eq.
        } catch (e) {
          return false;
        }
      });
    }
  };

  return {
    Validator: Validator
  };
})();
