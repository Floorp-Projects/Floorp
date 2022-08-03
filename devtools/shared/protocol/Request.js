/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
var { findPlaceholders, getPath } = require("devtools/shared/protocol/utils");
var { types } = require("devtools/shared/protocol/types");

/**
 * Manages a request template.
 *
 * @param object template
 *    The request template.
 * @construcor
 */
var Request = function(template = {}) {
  this.type = template.type;
  this.template = template;
  this.args = findPlaceholders(template, Arg);
};

Request.prototype = {
  /**
   * Write a request.
   *
   * @param array fnArgs
   *    The function arguments to place in the request.
   * @param object ctx
   *    The object making the request.
   * @returns a request packet.
   */
  write(fnArgs, ctx) {
    const ret = {};
    for (const key in this.template) {
      const value = this.template[key];
      if (value instanceof Arg) {
        ret[key] = value.write(
          value.index in fnArgs ? fnArgs[value.index] : undefined,
          ctx,
          key
        );
      } else if (key == "type") {
        ret[key] = value;
      } else {
        throw new Error(
          "Request can only an object with `Arg` or `Option` properties"
        );
      }
    }
    return ret;
  },

  /**
   * Read a request.
   *
   * @param object packet
   *    The request packet.
   * @param object ctx
   *    The object making the request.
   * @returns an arguments array
   */
  read(packet, ctx) {
    const fnArgs = [];
    for (const templateArg of this.args) {
      const arg = templateArg.placeholder;
      const path = templateArg.path;
      const name = path[path.length - 1];
      arg.read(getPath(packet, path), ctx, fnArgs, name);
    }
    return fnArgs;
  },
};

exports.Request = Request;

/**
 * Request/Response templates and generation
 *
 * Request packets are specified as json templates with
 * Arg and Option placeholders where arguments should be
 * placed.
 *
 * Reponse packets are also specified as json templates,
 * with a RetVal placeholder where the return value should be
 * placed.
 */

/**
 * Placeholder for simple arguments.
 *
 * @param number index
 *    The argument index to place at this position.
 * @param type type
 *    The argument should be marshalled as this type.
 * @constructor
 */
var Arg = function(index, type) {
  this.index = index;
  // Prevent force loading all Arg types by accessing it only when needed
  loader.lazyGetter(this, "type", function() {
    return types.getType(type);
  });
};

Arg.prototype = {
  write(arg, ctx) {
    return this.type.write(arg, ctx);
  },

  read(v, ctx, outArgs) {
    outArgs[this.index] = this.type.read(v, ctx);
  },
};

// Outside of protocol.js, Arg is called as factory method, without the new keyword.
exports.Arg = function(index, type) {
  return new Arg(index, type);
};

/**
 * Placeholder for an options argument value that should be hoisted
 * into the packet.
 *
 * If provided in a method specification:
 *
 *   { optionArg: Option(1)}
 *
 * Then arguments[1].optionArg will be placed in the packet in this
 * value's place.
 *
 * @param number index
 *    The argument index of the options value.
 * @param type type
 *    The argument should be marshalled as this type.
 * @constructor
 */
var Option = function(index, type) {
  Arg.call(this, index, type);
};

Option.prototype = extend(Arg.prototype, {
  write(arg, ctx, name) {
    // Ignore if arg is undefined or null; allow other falsy values
    if (arg == undefined || arg[name] == undefined) {
      return undefined;
    }
    const v = arg[name];
    return this.type.write(v, ctx);
  },
  read(v, ctx, outArgs, name) {
    if (outArgs[this.index] === undefined) {
      outArgs[this.index] = {};
    }
    if (v === undefined) {
      return;
    }
    outArgs[this.index][name] = this.type.read(v, ctx);
  },
});

// Outside of protocol.js, Option is called as factory method, without the new keyword.
exports.Option = function(index, type) {
  return new Option(index, type);
};
