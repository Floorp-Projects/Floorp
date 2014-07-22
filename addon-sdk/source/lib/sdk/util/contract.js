/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { validateOptions: valid } = require("../deprecated/api-utils");
const method = require("method/core");

// Function takes property validation rules and returns function that given
// an `options` object will return validated / normalized options back. If
// option(s) are invalid validator will throw exception described by rules.
// Returned will also have contain `rules` property with a given validation
// rules and `properties` function that can be used to generate validated
// property getter and setters can be mixed into prototype. For more details
// see `properties` function below.
function contract(rules) {
  const validator = (instance, options) => {
    return valid(options || instance || {}, rules);
  };
  validator.rules = rules
  validator.properties = function(modelFor) {
    return properties(modelFor, rules);
  }
  return validator;
}
exports.contract = contract

// Function takes `modelFor` instance state model accessor functions and
// a property validation rules and generates object with getters and setters
// that can be mixed into prototype. Property accessors update model for the
// given instance. If you wish to react to property updates you can always
// override setters to put specific logic.
function properties(modelFor, rules) {
  let descriptor = Object.keys(rules).reduce(function(descriptor, name) {
    descriptor[name] = {
      get: function() { return modelFor(this)[name] },
      set: function(value) {
        let change = {};
        change[name] = value;
        modelFor(this)[name] = valid(change, rules)[name];
      }
    }
    return descriptor
  }, {});
  return Object.create(Object.prototype, descriptor);
}
exports.properties = properties;

const validate = method("contract/validate");
exports.validate = validate;
