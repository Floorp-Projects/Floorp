/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

module.metadata = {
  "stability": "experimental"
};


const { Cc, Ci, CC } = require("chrome");
const { Class } = require("../core/heritage");

const Transcoder = CC("@mozilla.org/intl/scriptableunicodeconverter",
                      "nsIScriptableUnicodeConverter");

var Buffer = Class({
  initialize: function initialize(subject, encoding) {
    subject = subject ? subject.valueOf() : 0;
    let length = typeof subject === "number" ? subject : 0;
    this.encoding = encoding || "utf-8";
    this.valueOf(Array.isArray(subject) ? subject : new Array(length));

    if (typeof subject === "string") this.write(subject);
  },
  get length() {
    return this.valueOf().length;
  },
  get: function get(index) {
    return this.valueOf()[index];
  },
  set: function set(index, value) {
    return this.valueOf()[index] = value;
  },
  valueOf: function valueOf(value) {
    Object.defineProperty(this, "valueOf", {
      value: Array.prototype.valueOf.bind(value),
      configurable: false,
      writable: false,
      enumerable: false
    });
  },
  toString: function toString(encoding, start, end) {
    let bytes = this.valueOf().slice(start || 0, end || this.length);
    let transcoder = Transcoder();
    transcoder.charset = String(encoding || this.encoding).toUpperCase();
    return transcoder.convertFromByteArray(bytes, this.length);
  },
  toJSON: function toJSON() {
    return this.toString()
  },
  write: function write(string, offset, encoding) {
    offset = Math.max(offset || 0, 0);
    let value = this.valueOf();
    let transcoder = Transcoder();
    transcoder.charset = String(encoding || this.encoding).toUpperCase();
    let bytes = transcoder.convertToByteArray(string, {});
    value.splice.apply(value, [
      offset,
      Math.min(value.length - offset, bytes.length, bytes)
    ].concat(bytes));
    return bytes;
  },
  slice: function slice(start, end) {
    return new Buffer(this.valueOf().slice(start, end));
  },
  copy: function copy(target, offset, start, end) {
    offset = Math.max(offset || 0, 0);
    target = target.valueOf();
    let bytes = this.valueOf();
    bytes.slice(Math.max(start || 0, 0), end);
    target.splice.apply(target, [
      offset,
      Math.min(target.length - offset, bytes.length),
    ].concat(bytes));
  }
});
Buffer.isBuffer = function isBuffer(buffer) {
  return buffer instanceof Buffer
};
exports.Buffer = Buffer;
