/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'experimental'
};

/*
 * Encodings supported by TextEncoder/Decoder:
 * utf-8, utf-16le, utf-16be
 * http://encoding.spec.whatwg.org/#interface-textencoder
 *
 * Node however supports the following encodings:
 * ascii, utf-8, utf-16le, usc2, base64, hex
 */

const { Cu } = require('chrome');
const { isNumber } = require('sdk/lang/type');
const { TextEncoder, TextDecoder } = Cu.import('resource://gre/modules/commonjs/toolkit/loader.js', {});

exports.TextEncoder = TextEncoder;
exports.TextDecoder = TextDecoder;

/**
 * Use WeakMaps to work around Bug 929146, which prevents us from adding
 * getters or values to typed arrays
 * https://bugzilla.mozilla.org/show_bug.cgi?id=929146
 */
const parents = new WeakMap();
const views = new WeakMap();

function Buffer(subject, encoding /*, bufferLength */) {

  // Allow invocation without `new` constructor
  if (!(this instanceof Buffer))
    return new Buffer(subject, encoding, arguments[2]);

  var type = typeof(subject);

  switch (type) {
    case 'number':
      // Create typed array of the given size if number.
      try {
        let buffer = new Uint8Array(subject > 0 ? Math.floor(subject) : 0);
        return buffer;
      } catch (e) {
        if (/size and count too large/.test(e.message) ||
            /invalid arguments/.test(e.message))
          throw new RangeError('Could not instantiate buffer: size of buffer may be too large');
        else
          throw new Error('Could not instantiate buffer');
      }
      break;
    case 'string':
      // If string encode it and use buffer for the returned Uint8Array
      // to create a local patched version that acts like node buffer.
      encoding = encoding || 'utf8';
      return new Uint8Array(new TextEncoder(encoding).encode(subject).buffer);
    case 'object':
      // This form of the constructor uses the form of
      // new Uint8Array(buffer, offset, length);
      // So we can instantiate a typed array within the constructor
      // to inherit the appropriate properties, where both the
      // `subject` and newly instantiated buffer share the same underlying
      // data structure.
      if (arguments.length === 3)
        return new Uint8Array(subject, encoding, arguments[2]);
      // If array or alike just make a copy with a local patched prototype.
      else
        return new Uint8Array(subject);
    default:
      throw new TypeError('must start with number, buffer, array or string');
  }
}
exports.Buffer = Buffer;

// Tests if `value` is a Buffer.
Buffer.isBuffer = value => value instanceof Buffer

// Returns true if the encoding is a valid encoding argument & false otherwise
Buffer.isEncoding = function (encoding) {
  if (!encoding) return false;
  try {
    new TextDecoder(encoding);
  } catch(e) {
    return false;
  }
  return true;
}

// Gives the actual byte length of a string. encoding defaults to 'utf8'.
// This is not the same as String.prototype.length since that returns the
// number of characters in a string.
Buffer.byteLength = (value, encoding = 'utf8') =>
  new TextEncoder(encoding).encode(value).byteLength

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
// access by index. Since in SDK each module has it's own set of bult-ins it
// ok to patch ours to make it nodejs Buffer compatible.
Buffer.prototype = Uint8Array.prototype;
Object.defineProperties(Buffer.prototype, {
  parent: {
    get: function() { return parents.get(this, undefined); }
  },
  view: {
    get: function () {
      let view = views.get(this, undefined);
      if (view) return view;
      view = new DataView(this.buffer);
      views.set(this, view);
      return view;
    }
  },
  toString: {
    value: function(encoding, start, end) {
      encoding = !!encoding ? (encoding + '').toLowerCase() : 'utf8';
      start = Math.max(0, ~~start);
      end = Math.min(this.length, end === void(0) ? this.length : ~~end);
      return new TextDecoder(encoding).decode(this.subarray(start, end));
    }
  },
  toJSON: {
    value: function() {
      return { type: 'Buffer', data: Array.slice(this, 0) };
    }
  },
  get: {
    value: function(offset) {
      return this[offset];
    }
  },
  set: {
    value: function(offset, value) { this[offset] = value; }
  },
  copy: {
    value: function(target, offset, start, end) {
      let length = this.length;
      let targetLength = target.length;
      offset = isNumber(offset) ? offset : 0;
      start = isNumber(start) ? start : 0;

      if (start < 0)
        throw new RangeError('sourceStart is outside of valid range');
      if (end < 0)
        throw new RangeError('sourceEnd is outside of valid range');

      // If sourceStart > sourceEnd, or targetStart > targetLength,
      // zero bytes copied
      if (start > end ||
          offset > targetLength
          )
        return 0;

      // If `end` is not defined, or if it is defined
      // but would overflow `target`, redefine `end`
      // so we can copy as much as we can
      if (end - start > targetLength - offset ||
          end == null) {
        let remainingTarget = targetLength - offset;
        let remainingSource = length - start;
        if (remainingSource <= remainingTarget)
          end = length;
        else
          end = start + remainingTarget;
      }

      Uint8Array.set(target, this.subarray(start, end), offset);
      return end - start;
    }
  },
  slice: {
    value: function(start, end) {
      let length = this.length;
      start = ~~start;
      end = end != null ? end : length;

      if (start < 0) {
        start += length;
        if (start < 0) start = 0;
      } else if (start > length)
        start = length;

      if (end < 0) {
        end += length;
        if (end < 0) end = 0;
      } else if (end > length)
        end = length;

      if (end < start)
        end = start;

      // This instantiation uses the new Uint8Array(buffer, offset, length) version
      // of construction to share the same underling data structure
      let buffer = new Buffer(this.buffer, start, end - start);

      // If buffer has a value, assign its parent value to the
      // buffer it shares its underlying structure with. If a slice of
      // a slice, then use the root structure
      if (buffer.length > 0)
        parents.set(buffer, this.parent || this);

      return buffer;
    }
  },
  write: {
    value: function(string, offset, length, encoding = 'utf8') {
      // write(string, encoding);
      if (typeof(offset) === 'string' && Number.isNaN(parseInt(offset))) {
        ([offset, length, encoding]) = [0, null, offset];
      }
      // write(string, offset, encoding);
      else if (typeof(length) === 'string')
        ([length, encoding]) = [null, length];

      if (offset < 0 || offset > this.length)
        throw new RangeError('offset is outside of valid range');

      offset = ~~offset;

      // Clamp length if it would overflow buffer, or if its
      // undefined
      if (length == null || length + offset > this.length)
        length = this.length - offset;

      let buffer = new TextEncoder(encoding).encode(string);
      let result = Math.min(buffer.length, length);
      if (buffer.length !== length)
        buffer = buffer.subarray(0, length);

      Uint8Array.set(this, buffer, offset);
      return result;
    }
  },
  fill: {
    value: function fill(value, start, end) {
      let length = this.length;
      value = value || 0;
      start = start || 0;
      end = end || length;

      if (typeof(value) === 'string')
        value = value.charCodeAt(0);
      if (typeof(value) !== 'number' || isNaN(value))
        throw TypeError('value is not a number');
      if (end < start)
        throw new RangeError('end < start');

      // Fill 0 bytes; we're done
      if (end === start)
        return 0;
      if (length == 0)
        return 0;

      if (start < 0 || start >= length)
        throw RangeError('start out of bounds');

      if (end < 0 || end > length)
        throw RangeError('end out of bounds');

      let index = start;
      while (index < end) this[index++] = value;
    }
  }
});

// Define nodejs Buffer's getter and setter functions that just proxy
// to internal DataView's equivalent methods.

// TODO do we need to check architecture to see if it's default big/little endian?
[['readUInt16LE', 'getUint16', true],
 ['readUInt16BE', 'getUint16', false],
 ['readInt16LE', 'getInt16', true],
 ['readInt16BE', 'getInt16', false],
 ['readUInt32LE', 'getUint32', true],
 ['readUInt32BE', 'getUint32', false],
 ['readInt32LE', 'getInt32', true],
 ['readInt32BE', 'getInt32', false],
 ['readFloatLE', 'getFloat32', true],
 ['readFloatBE', 'getFloat32', false],
 ['readDoubleLE', 'getFloat64', true],
 ['readDoubleBE', 'getFloat64', false],
 ['readUInt8', 'getUint8'],
 ['readInt8', 'getInt8']].forEach(([alias, name, littleEndian]) => {
  Object.defineProperty(Buffer.prototype, alias, {
    value: function(offset) this.view[name](offset, littleEndian)
  });
});

[['writeUInt16LE', 'setUint16', true],
 ['writeUInt16BE', 'setUint16', false],
 ['writeInt16LE', 'setInt16', true],
 ['writeInt16BE', 'setInt16', false],
 ['writeUInt32LE', 'setUint32', true],
 ['writeUInt32BE', 'setUint32', false],
 ['writeInt32LE', 'setInt32', true],
 ['writeInt32BE', 'setInt32', false],
 ['writeFloatLE', 'setFloat32', true],
 ['writeFloatBE', 'setFloat32', false],
 ['writeDoubleLE', 'setFloat64', true],
 ['writeDoubleBE', 'setFloat64', false],
 ['writeUInt8', 'setUint8'],
 ['writeInt8', 'setInt8']].forEach(([alias, name, littleEndian]) => {
  Object.defineProperty(Buffer.prototype, alias, {
    value: function(value, offset) this.view[name](offset, value, littleEndian)
  });
});
