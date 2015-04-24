/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const { merge } = require("../util/object");
const { union } = require("../util/array");
const { isNil, isRegExp } = require("../lang/type");

// The possible return values of getTypeOf.
const VALID_TYPES = [
  "array",
  "boolean",
  "function",
  "null",
  "number",
  "object",
  "string",
  "undefined",
  "regexp"
];

const { isArray } = Array;

/**
 * Returns a validated options dictionary given some requirements.  If any of
 * the requirements are not met, an exception is thrown.
 *
 * @param  options
 *         An object, the options dictionary to validate.  It's not modified.
 *         If it's null or otherwise falsey, an empty object is assumed.
 * @param  requirements
 *         An object whose keys are the expected keys in options.  Any key in
 *         options that is not present in requirements is ignored.  Each value
 *         in requirements is itself an object describing the requirements of
 *         its key.  There are four optional keys in this object:
 *           map: A function that's passed the value of the key in options.
 *                map's return value is taken as the key's value in the final
 *                validated options, is, and ok.  If map throws an exception
 *                it's caught and discarded, and the key's value is its value in
 *                options.
 *           is:  An array containing any number of the typeof type names.  If
 *                the key's value is none of these types, it fails validation.
 *                Arrays, null and regexps are identified by the special type names
 *                "array", "null", "regexp"; "object" will not match either.  No type
 *                coercion is done.
 *           ok:  A function that's passed the key's value.  If it returns
 *                false, the value fails validation.
 *           msg: If the key's value fails validation, an exception is thrown.
 *                This string will be used as its message.  If undefined, a
 *                generic message is used, unless is is defined, in which case
 *                the message will state that the value needs to be one of the
 *                given types.
 * @return An object whose keys are those keys in requirements that are also in
 *         options and whose values are the corresponding return values of map
 *         or the corresponding values in options.  Note that any keys not
 *         shared by both requirements and options are not in the returned
 *         object.
 */
exports.validateOptions = function validateOptions(options, requirements) {
  options = options || {};
  let validatedOptions = {};

  for (let key in requirements) {
    let isOptional = false;
    let mapThrew = false;
    let req = requirements[key];
    let [optsVal, keyInOpts] = (key in options) ?
                               [options[key], true] :
                               [undefined, false];
    if (req.map) {
      try {
        optsVal = req.map(optsVal);
      }
      catch (err) {
        if (err instanceof RequirementError)
          throw err;

        mapThrew = true;
      }
    }
    if (req.is) {
      let types = req.is;

      if (!isArray(types) && isArray(types.is))
        types = types.is;

      if (isArray(types)) {
        isOptional = ['undefined', 'null'].every(v => ~types.indexOf(v));

        // Sanity check the caller's type names.
        types.forEach(function (typ) {
          if (VALID_TYPES.indexOf(typ) < 0) {
            let msg = 'Internal error: invalid requirement type "' + typ + '".';
            throw new Error(msg);
          }
        });
        if (types.indexOf(getTypeOf(optsVal)) < 0)
          throw new RequirementError(key, req);
      }
    }

    if (req.ok && ((!isOptional || !isNil(optsVal)) && !req.ok(optsVal)))
      throw new RequirementError(key, req);

    if (keyInOpts || (req.map && !mapThrew && optsVal !== undefined))
      validatedOptions[key] = optsVal;
  }

  return validatedOptions;
};

exports.addIterator = function addIterator(obj, keysValsGenerator) {
  obj.__iterator__ = function(keysOnly, keysVals) {
    let keysValsIterator = keysValsGenerator.call(this);

    // "for (.. in ..)" gets only keys, "for each (.. in ..)" gets values,
    // and "for (.. in Iterator(..))" gets [key, value] pairs.
    let index = keysOnly ? 0 : 1;
    while (true)
      yield keysVals ? keysValsIterator.next() : keysValsIterator.next()[index];
  };
};

// Similar to typeof, except arrays, null and regexps are identified by "array" and
// "null" and "regexp", not "object".
let getTypeOf = exports.getTypeOf = function getTypeOf(val) {
  let typ = typeof(val);
  if (typ === "object") {
    if (!val)
      return "null";
    if (isArray(val))
      return "array";
    if (isRegExp(val))
      return "regexp";
  }
  return typ;
}

function RequirementError(key, requirement) {
  Error.call(this);

  this.name = "RequirementError";

  let msg = requirement.msg;
  if (!msg) {
    msg = 'The option "' + key + '" ';
    msg += requirement.is ?
           "must be one of the following types: " + requirement.is.join(", ") :
           "is invalid.";
  }

  this.message = msg;
}
RequirementError.prototype = Object.create(Error.prototype);

let string = { is: ['string', 'undefined', 'null'] };
exports.string = string;

let number = { is: ['number', 'undefined', 'null'] };
exports.number = number;

let boolean = { is: ['boolean', 'undefined', 'null'] };
exports.boolean = boolean;

let object = { is: ['object', 'undefined', 'null'] };
exports.object = object;

let array = { is: ['array', 'undefined', 'null'] };
exports.array = array;

let isTruthyType = type => !(type === 'undefined' || type === 'null');
let findTypes = v => { while (!isArray(v) && v.is) v = v.is; return v };

function required(req) {
  let types = (findTypes(req) || VALID_TYPES).filter(isTruthyType);

  return merge({}, req, {is: types});
}
exports.required = required;

function optional(req) {
  req = merge({is: []}, req);
  req.is = findTypes(req).filter(isTruthyType).concat('undefined', 'null');

  return req;
}
exports.optional = optional;

function either(...types) {
  return union.apply(null, types.map(findTypes));
}
exports.either = either;
