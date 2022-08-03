/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { findPlaceholders, getPath } = require("devtools/shared/protocol/utils");
var { types } = require("devtools/shared/protocol/types");

/**
 * Manages a response template.
 *
 * @param object template
 *    The response template.
 * @construcor
 */
var Response = function(template = {}) {
  this.template = template;
  if (this.template instanceof RetVal && this.template.isArrayType()) {
    throw Error("Arrays should be wrapped in objects");
  }

  const placeholders = findPlaceholders(template, RetVal);
  if (placeholders.length > 1) {
    throw Error("More than one RetVal specified in response");
  }
  const placeholder = placeholders.shift();
  if (placeholder) {
    this.retVal = placeholder.placeholder;
    this.path = placeholder.path;
  }
};

Response.prototype = {
  /**
   * Write a response for the given return value.
   *
   * @param val ret
   *    The return value.
   * @param object ctx
   *    The object writing the response.
   */
  write(ret, ctx) {
    // Consider that `template` is either directly a `RetVal`,
    // or a dictionary with may be one `RetVal`.
    if (this.template instanceof RetVal) {
      return this.template.write(ret, ctx);
    }
    const result = {};
    for (const key in this.template) {
      const value = this.template[key];
      if (value instanceof RetVal) {
        result[key] = value.write(ret, ctx);
      } else {
        throw new Error(
          "Response can only be a `RetVal` instance or an object " +
            "with one property being a `RetVal` instance."
        );
      }
    }
    return result;
  },

  /**
   * Read a return value from the given response.
   *
   * @param object packet
   *    The response packet.
   * @param object ctx
   *    The object reading the response.
   */
  read(packet, ctx) {
    if (!this.retVal) {
      return undefined;
    }
    const v = getPath(packet, this.path);
    return this.retVal.read(v, ctx);
  },
};

exports.Response = Response;

/**
 * Placeholder for return values in a response template.
 *
 * @param type type
 *    The return value should be marshalled as this type.
 */
var RetVal = function(type) {
  this._type = type;
  // Prevent force loading all RetVal types by accessing it only when needed
  loader.lazyGetter(this, "type", function() {
    return types.getType(type);
  });
};

RetVal.prototype = {
  write(v, ctx) {
    return this.type.write(v, ctx);
  },

  read(v, ctx) {
    return this.type.read(v, ctx);
  },

  isArrayType() {
    // `_type` should always be a string, but a few incorrect RetVal calls
    // pass `0`. See Bug 1677703.
    return typeof this._type === "string" && this._type.startsWith("array:");
  },
};

// Outside of protocol.js, RetVal is called as factory method, without the new keyword.
exports.RetVal = function(type) {
  return new RetVal(type);
};
