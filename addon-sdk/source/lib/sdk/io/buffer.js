/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};


const { Cu } = require("chrome");
const { TextEncoder, TextDecoder } = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});

exports.TextEncoder = TextEncoder;
exports.TextDecoder = TextDecoder;


function Buffer(subject, encoding) {
    var type = typeof(subject);
    switch (type) {
      case "number":
        // Create typed array of the given size if number.
        return Uint8Array(subject > 0 ? Math.floor(subject) : 0);
      case "string":
        // If string encode it and use buffer for the returned Uint8Array
        // to create a local patched version that acts like node buffer.
        encoding = encoding || "utf8";
        return Uint8Array(TextEncoder(encoding).encode(subject).buffer);
      case "object":
        // If array or alike just make a copy with a local patched prototype.
        return Uint8Array(subject);
     default:
        throw new TypeError("must start with number, buffer, array or string");
    }
}
exports.Buffer = Buffer;

// Tests if `value` is a Buffer.
Buffer.isBuffer = value => value instanceof Buffer

// Returns true if the encoding is a valid encoding argument & false otherwise
Buffer.isEncoding = encoding => !!ENCODINGS[String(encoding).toLowerCase()]

// Gives the actual byte length of a string. encoding defaults to 'utf8'.
// This is not the same as String.prototype.length since that returns the
// number of characters in a string.
Buffer.byteLength = (value, encoding = "utf8") =>
  TextEncoder(encoding).encode(value).byteLength

// Direct copy of the nodejs's buffer implementation:
// https://github.com/joyent/node/blob/b255f4c10a80343f9ce1cee56d0288361429e214/lib/buffer.js#L146-L177
Buffer.concat = function(list, length) {
  if (!Array.isArray(list))
    throw new TypeError('Usage: Buffer.concat(list[, length])');

  if (typeof length === 'undefined') {
    length = 0;
    for (var i = 0; i < list.length; i++)
      length += list[i].length;
  } else {
    length = ~~length;
  }

  if (length < 0)
    length = 0;

  if (list.length === 0)
    return new Buffer(0);
  else if (list.length === 1)
    return list[0];

  if (length < 0)
    throw new RangeError('length is not a positive number');

  var buffer = new Buffer(length);
  var pos = 0;
  for (var i = 0; i < list.length; i++) {
    var buf = list[i];
    buf.copy(buffer, pos);
    pos += buf.length;
  }

  return buffer;
};

// Node buffer is very much like Uint8Array although it has bunch of methods
// that typically can be used in combination with `DataView` while preserving
// access by index. Since in SDK echo module has it's own set of bult-ins it
// ok to patch ours to make it nodejs Buffer compatible.
Buffer.prototype = Uint8Array.prototype;
Object.defineProperties(Buffer.prototype, {
  view: {
    get: function() this._view || (this._view = DataView(this.buffer))
  },
  toString: {
    value: function(encoding, start, end) {
      encoding = !!encoding ? (encoding + '').toLowerCase() : "utf8";
      start = Math.max(0, ~~start);
      end = Math.min(this.length, end === void(0) ? this.length : ~~end);
      return TextDecoder(encoding).decode(this.subarray(start, end));
    }
  },
  toJSON: {
    value: function() ({ type: "Buffer", data: Array.slice(this, 0) })
  },
  get: {
    value: function(offset) this[offset]
  },
  set: {
    value: function(offset, value) this[offset] = value
  },
  copy: {
    value: function(target, offset, start, end)
      Uint8Array.set(target, this.subarray(start, end), offset)
  },
  slice: {
    value: Buffer.prototype.subarray
  },
  write: {
    value: function(string, offset, length, encoding = "utf8") {
      if (typeof(offset) === "string")
        ([offset, length, encoding]) = [0, null, offset];
      else if (typeof(length) === "string")
        ([length, encoding]) = [null, length];

      offset = ~~offset;
      length = length || this.length - offset;
      let buffer = TextEncoder(encoding).encode(string);
      let result = Math.min(buffer.length, length);
      if (buffer.length !== length)
        buffer = buffer.subarray(0, length);
      Uint8Array.set(this, buffer, offset);
      return result;
    }
  },
  fill: {
    value: function fill(value, start, end) {
      value = value || 0;
      start = start || 0;
      end = end || this.length;

      if (typeof(value) === "string")
        value = value.charCodeAt(0);
      if (typeof(value) !== "number" || isNaN(value))
        throw TypeError("value is not a number");
      if (end < start)
        throw new RangeError("end < start");

      // Fill 0 bytes; we're done
      if (end === start)
        return 0;
      if (this.length == 0)
        return 0;

      if (start < 0 || start >= this.length)
        throw RangeError("start out of bounds");

      if (end < 0 || end > this.length)
        throw RangeError("end out of bounds");

      let index = start;
      while (index < end) this[index++] = value;
    }
  }
});

// Define nodejs Buffer's getter and setter functions that just proxy
// to internal DataView's equivalent methods.
[["readUInt16LE", "getUint16", true],
 ["readUInt16BE", "getUint16", false],
 ["readInt16LE", "getInt16", true],
 ["readInt16BE", "getInt16", false],
 ["readUInt32LE", "getInt32", true],
 ["readUInt32BE", "getInt32", false],
 ["readInt32LE", "getInt32", true],
 ["readInt32BE", "getInt32", false],
 ["readFloatLE", "getFloat32", true],
 ["readFloatBE", "getFloat32", false],
 ["readDoubleLE", "getFloat64", true],
 ["readDoubleBE", "getFloat64", false],
 ["readUInt8", "getUint8"],
 ["readInt8", "getInt8"]].forEach(([alias, name, littleEndian]) => {
  Object.defineProperty(Buffer.prototype, alias, {
    value: function(offset) this.view[name](offset, littleEndian)
  });
});

[["writeUInt16LE", "setUint16", true],
 ["writeUInt16BE", "setUint16", false],
 ["writeInt16LE", "setInt16", true],
 ["writeInt16BE", "setInt16", false],
 ["writeUInt32LE", "setUint32", true],
 ["writeUInt32BE", "setUint32", false],
 ["writeInt32LE", "setInt32", true],
 ["writeInt32BE", "setInt32", false],
 ["writeFloatLE", "setFloat32", true],
 ["writeFloatBE", "setFloat32", false],
 ["writeDoubleLE", "setFloat64", true],
 ["writeDoubleBE", "setFloat64", false],
 ["writeUInt8", "setUint8"],
 ["writeInt8", "setInt8"]].forEach(([alias, name, littleEndian]) => {
  Object.defineProperty(Buffer.prototype, alias, {
    value: function(value, offset) this.view[name](offset, value, littleEndian)
  });
});


// List of supported encodings taken from:
// http://mxr.mozilla.org/mozilla-central/source/dom/encoding/labelsencodings.properties
const ENCODINGS = { "unicode-1-1-utf-8": 1, "utf-8": 1, "utf8": 1,
  "866": 1, "cp866": 1, "csibm866": 1, "ibm866": 1, "csisolatin2": 1,
  "iso-8859-2": 1, "iso-ir-101": 1, "iso8859-2": 1, "iso88592": 1,
  "iso_8859-2": 1, "iso_8859-2:1987": 1, "l2": 1, "latin2": 1, "csisolatin3": 1,
  "iso-8859-3": 1, "iso-ir-109": 1, "iso8859-3": 1, "iso88593": 1,
  "iso_8859-3": 1, "iso_8859-3:1988": 1, "l3": 1, "latin3": 1, "csisolatin4": 1,
  "iso-8859-4": 1, "iso-ir-110": 1, "iso8859-4": 1, "iso88594": 1,
  "iso_8859-4": 1, "iso_8859-4:1988": 1, "l4": 1, "latin4": 1,
  "csisolatincyrillic": 1, "cyrillic": 1, "iso-8859-5": 1, "iso-ir-144": 1,
  "iso8859-5": 1, "iso88595": 1, "iso_8859-5": 1, "iso_8859-5:1988": 1,
  "arabic": 1, "asmo-708": 1, "csiso88596e": 1, "csiso88596i": 1,
  "csisolatinarabic": 1, "ecma-114": 1, "iso-8859-6": 1, "iso-8859-6-e": 1,
  "iso-8859-6-i": 1, "iso-ir-127": 1, "iso8859-6": 1, "iso88596": 1,
  "iso_8859-6": 1, "iso_8859-6:1987": 1, "csisolatingreek": 1, "ecma-118": 1,
  "elot_928": 1, "greek": 1, "greek8": 1, "iso-8859-7": 1, "iso-ir-126": 1,
  "iso8859-7": 1, "iso88597": 1, "iso_8859-7": 1, "iso_8859-7:1987": 1,
  "sun_eu_greek": 1, "csiso88598e": 1, "csisolatinhebrew": 1, "hebrew": 1,
  "iso-8859-8": 1, "iso-8859-8-e": 1, "iso-ir-138": 1, "iso8859-8": 1,
  "iso88598": 1, "iso_8859-8": 1, "iso_8859-8:1988": 1, "visual": 1,
  "csiso88598i": 1, "iso-8859-8-i": 1, "logical": 1, "csisolatin6": 1,
  "iso-8859-10": 1, "iso-ir-157": 1, "iso8859-10": 1, "iso885910": 1,
  "l6": 1, "latin6": 1, "iso-8859-13": 1, "iso8859-13": 1, "iso885913": 1,
  "iso-8859-14": 1, "iso8859-14": 1, "iso885914": 1, "csisolatin9": 1,
  "iso-8859-15": 1, "iso8859-15": 1, "iso885915": 1, "iso_8859-15": 1,
  "l9": 1, "iso-8859-16": 1, "cskoi8r": 1, "koi": 1, "koi8": 1, "koi8-r": 1,
  "koi8_r": 1, "koi8-u": 1, "csmacintosh": 1, "mac": 1, "macintosh": 1,
  "x-mac-roman": 1, "dos-874": 1, "iso-8859-11": 1, "iso8859-11": 1,
  "iso885911": 1, "tis-620": 1, "windows-874": 1, "cp1250": 1,
  "windows-1250": 1, "x-cp1250": 1, "cp1251": 1, "windows-1251": 1,
  "x-cp1251": 1, "ansi_x3.4-1968": 1, "ascii": 1, "cp1252": 1, "cp819": 1,
  "csisolatin1": 1, "ibm819": 1, "iso-8859-1": 1, "iso-ir-100": 1,
  "iso8859-1": 1, "iso88591": 1, "iso_8859-1": 1, "iso_8859-1:1987": 1,
  "l1": 1, "latin1": 1, "us-ascii": 1, "windows-1252": 1, "x-cp1252": 1,
  "cp1253": 1, "windows-1253": 1, "x-cp1253": 1, "cp1254": 1, "csisolatin5": 1,
  "iso-8859-9": 1, "iso-ir-148": 1, "iso8859-9": 1, "iso88599": 1,
  "iso_8859-9": 1, "iso_8859-9:1989": 1, "l5": 1, "latin5": 1,
  "windows-1254": 1, "x-cp1254": 1, "cp1255": 1, "windows-1255": 1,
  "x-cp1255": 1, "cp1256": 1, "windows-1256": 1, "x-cp1256": 1, "cp1257": 1,
  "windows-1257": 1, "x-cp1257": 1, "cp1258": 1, "windows-1258": 1,
  "x-cp1258": 1, "x-mac-cyrillic": 1, "x-mac-ukrainian": 1, "chinese": 1,
  "csgb2312": 1, "csiso58gb231280": 1, "gb2312": 1, "gb_2312": 1,
  "gb_2312-80": 1, "gbk": 1, "iso-ir-58": 1, "x-gbk": 1, "gb18030": 1,
  "hz-gb-2312": 1, "big5": 1, "big5-hkscs": 1, "cn-big5": 1, "csbig5": 1,
  "x-x-big5": 1, "cseucpkdfmtjapanese": 1, "euc-jp": 1, "x-euc-jp": 1,
  "csiso2022jp": 1, "iso-2022-jp": 1, "csshiftjis": 1, "ms_kanji": 1,
  "shift-jis": 1, "shift_jis": 1, "sjis": 1, "windows-31j": 1, "x-sjis": 1,
  "cseuckr": 1, "csksc56011987": 1, "euc-kr": 1, "iso-ir-149": 1, "korean": 1,
  "ks_c_5601-1987": 1, "ks_c_5601-1989": 1, "ksc5601": 1, "ksc_5601": 1,
  "windows-949": 1, "csiso2022kr": 1, "iso-2022-kr": 1, "utf-16": 1,
  "utf-16le": 1, "utf-16be": 1, "x-user-defined": 1 };
