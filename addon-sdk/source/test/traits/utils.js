/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var ERR_CONFLICT = "Remaining conflicting property: ";
var ERR_REQUIRED = "Missing required property: ";

exports.Data = function Data(value, enumerable, configurable, writable) {
  return ({
    value: value,
    enumerable: enumerable !== false,
    configurable: configurable !== false,
    writable: writable !== false
  });
};

exports.Method = function Method(method, enumerable, configurable, writable) {
  return ({
    value: method,
    enumerable: enumerable !== false,
    configurable: configurable !== false,
    writable: writable !== false
  });
};

exports.Accessor = function Accessor(get, set, enumerable, configurable) {
  return ({
    get: get,
    set: set,
    enumerable: enumerable !== false,
    configurable: configurable !== false
  });
};

exports.Required = function Required(name) {
  function required() { throw new Error(ERR_REQUIRED + name) }

  return ({
    get: required,
    set: required,
    required: true
  });
};

exports.Conflict = function Conflict(name) {
  function conflict() { throw new Error(ERR_CONFLICT + name) }

  return ({
    get: conflict,
    set: conflict,
    conflict: true
  });
};

