/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * Many of these tests taken from Joyent's Node
 * https://github.com/joyent/node/blob/master/test/simple/test-buffer.js
 */

// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

const { Buffer, TextEncoder, TextDecoder } = require('sdk/io/buffer');
const { safeMerge } = require('sdk/util/object');

const ENCODINGS = ['utf-8', 'utf-16le', 'utf-16be'];

exports.testBufferMain = function (assert) {
  let b = Buffer('abcdef');

  // try to create 0-length buffers
  new Buffer('');
  new Buffer(0);
  // test encodings supported by node;
  // this is different than what node supports, details
  // in buffer.js
  ENCODINGS.forEach(enc => {
    new Buffer('', enc);
    assert.pass('Creating a buffer with ' + enc + ' does not throw');
  });

  ENCODINGS.forEach(function(encoding) {
    // Does not work with utf8
    if (encoding === 'utf-8') return;
    var b = new Buffer(10);
    b.write('あいうえお', encoding);
    assert.equal(b.toString(encoding), 'あいうえお',
      'encode and decodes buffer with ' + encoding);
  });

  // invalid encoding for Buffer.toString
  assert.throws(() => {
    b.toString('invalid');
  }, TypeError, 'invalid encoding for Buffer.toString');

  // try to toString() a 0-length slice of a buffer, both within and without the
  // valid buffer range
  assert.equal(new Buffer('abc').toString('utf8', 0, 0), '',
    'toString 0-length buffer, valid range');
  assert.equal(new Buffer('abc').toString('utf8', -100, -100), '',
    'toString 0-length buffer, invalid range');
  assert.equal(new Buffer('abc').toString('utf8', 100, 100), '',
    'toString 0-length buffer, invalid range');

  // try toString() with a object as a encoding
  assert.equal(new Buffer('abc').toString({toString: function() {
    return 'utf8';
  }}), 'abc', 'toString with object as an encoding');

  // test for buffer overrun
  var buf = new Buffer([0, 0, 0, 0, 0]); // length: 5
  var sub = buf.slice(0, 4); // length: 4
  var written = sub.write('12345', 'utf8');
  assert.equal(written, 4, 'correct bytes written in slice');
  assert.equal(buf[4], 0, 'correct origin buffer value');

  // Check for fractional length args, junk length args, etc.
  // https://github.com/joyent/node/issues/1758
  Buffer(3.3).toString(); // throws bad argument error in commit 43cb4ec
  assert.equal(Buffer(-1).length, 0);
  assert.equal(Buffer(NaN).length, 0);
  assert.equal(Buffer(3.3).length, 3);
  assert.equal(Buffer({length: 3.3}).length, 3);
  assert.equal(Buffer({length: 'BAM'}).length, 0);

  // Make sure that strings are not coerced to numbers.
  assert.equal(Buffer('99').length, 2);
  assert.equal(Buffer('13.37').length, 5);
};

exports.testIsEncoding = function (assert) {
  ENCODINGS.map(encoding => {
    assert.ok(Buffer.isEncoding(encoding),
      'Buffer.isEncoding ' + encoding + ' truthy');
  });
  ['not-encoding', undefined, null, 100, {}].map(encoding => {
    assert.ok(!Buffer.isEncoding(encoding),
      'Buffer.isEncoding ' + encoding + ' falsy');
  });
};

exports.testBufferCopy = function (assert) {
  // counter to ensure unique value is always copied
  var cntr = 0;
  var b = Buffer(1024); // safe constructor

  assert.strictEqual(1024, b.length);
  b[0] = -1;
  assert.strictEqual(b[0], 255);

  var shimArray = [];
  for (var i = 0; i < 1024; i++) {
    b[i] = i % 256;
    shimArray[i] = i % 256;
  }

  compareBuffers(assert, b, shimArray, 'array notation');

  var c = new Buffer(512);
  assert.strictEqual(512, c.length);
  // copy 512 bytes, from 0 to 512.
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = b.copy(c, 0, 0, 512);
  assert.strictEqual(512, copied,
    'copied ' + copied + ' bytes from b into c');

  compareBuffers(assert, b, c, 'copied to other buffer');

  // copy c into b, without specifying sourceEnd
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = c.copy(b, 0, 0);
  assert.strictEqual(c.length, copied,
    'copied ' + copied + ' bytes from c into b w/o sourceEnd');
  compareBuffers(assert, b, c,
    'copied to other buffer without specifying sourceEnd');

  // copy c into b, without specifying sourceStart
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = c.copy(b, 0);
  assert.strictEqual(c.length, copied,
    'copied ' + copied + ' bytes from c into b w/o sourceStart');
  compareBuffers(assert, b, c,
    'copied to other buffer without specifying sourceStart');

  // copy longer buffer b to shorter c without targetStart
  b.fill(++cntr);
  c.fill(++cntr);

  var copied = b.copy(c);
  assert.strictEqual(c.length, copied,
    'copied ' + copied + ' bytes from b into c w/o targetStart');
  compareBuffers(assert, b, c,
    'copy long buffer to shorter buffer without targetStart');

  // copy starting near end of b to c
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = b.copy(c, 0, b.length - Math.floor(c.length / 2));
  assert.strictEqual(Math.floor(c.length / 2), copied,
    'copied ' + copied + ' bytes from end of b into beg. of c');

  let successStatus = true;
  for (var i = 0; i < Math.floor(c.length / 2); i++) {
    if (b[b.length - Math.floor(c.length / 2) + i] !== c[i])
      successStatus = false;
  }

  for (var i = Math.floor(c.length /2) + 1; i < c.length; i++) {
    if (c[c.length-1] !== c[i])
      successStatus = false;
  }
  assert.ok(successStatus,
    'Copied bytes from end of large buffer into beginning of small buffer');

  // try to copy 513 bytes, and check we don't overrun c
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = b.copy(c, 0, 0, 513);
  assert.strictEqual(c.length, copied,
    'copied ' + copied + ' bytes from b trying to overrun c');
  compareBuffers(assert, b, c,
    'copying to buffer that would overflow');

  // copy 768 bytes from b into b
  b.fill(++cntr);
  b.fill(++cntr, 256);
  var copied = b.copy(b, 0, 256, 1024);
  assert.strictEqual(768, copied,
    'copied ' + copied + ' bytes from b into b');

  compareBuffers(assert, b, shimArray.map(()=>cntr),
    'copy partial buffer to itself');

  // copy string longer than buffer length (failure will segfault)
  var bb = new Buffer(10);
  bb.fill('hello crazy world');

  // copy throws at negative sourceStart
  assert.throws(function() {
    Buffer(5).copy(Buffer(5), 0, -1);
  }, RangeError, 'buffer copy throws at negative sourceStart');

  // check sourceEnd resets to targetEnd if former is greater than the latter
  b.fill(++cntr);
  c.fill(++cntr);
  var copied = b.copy(c, 0, 0, 1025);
  assert.strictEqual(copied, c.length,
    'copied ' + copied + ' bytes from b into c');
  compareBuffers(assert, b, c, 'copying should reset sourceEnd if targetEnd if sourceEnd > targetEnd');

  // throw with negative sourceEnd
  assert.throws(function() {
    b.copy(c, 0, 0, -1);
  }, RangeError, 'buffer copy throws at negative sourceEnd');

  // when sourceStart is greater than sourceEnd, zero copied
  assert.equal(b.copy(c, 0, 100, 10), 0);

  // when targetStart > targetLength, zero copied
  assert.equal(b.copy(c, 512, 0, 10), 0);

  // try to copy 0 bytes worth of data into an empty buffer
  b.copy(new Buffer(0), 0, 0, 0);

  // try to copy 0 bytes past the end of the target buffer
  b.copy(new Buffer(0), 1, 1, 1);
  b.copy(new Buffer(1), 1, 1, 1);

  // try to copy 0 bytes from past the end of the source buffer
  b.copy(new Buffer(1), 0, 2048, 2048);
};

exports.testBufferWrite = function (assert) {
  let b = Buffer(1024);
  b.fill(0);

  assert.throws(() => {
    b.write('test string', 0, 5, 'invalid');
  }, TypeError, 'invalid encoding with buffer write throws');
  // try to write a 0-length string beyond the end of b
  assert.throws(function() {
    b.write('', 2048);
  }, RangeError, 'writing a 0-length string beyond buffer throws');
  // throw when writing to negative offset
  assert.throws(function() {
    b.write('a', -1);
  }, RangeError, 'writing negative offset on buffer throws');

  // throw when writing past bounds from the pool
  assert.throws(function() {
    b.write('a', 2048);
  }, RangeError, 'writing past buffer bounds from pool throws');

  // testing for smart defaults and ability to pass string values as offset

  // previous write API was the following:
  // write(string, encoding, offset, length)
  // this is planned on being removed in node v0.13,
  // we will not support it
  var writeTest = new Buffer('abcdes');
  writeTest.write('n', 'utf8');
//  writeTest.write('o', 'utf8', '1');
  writeTest.write('d', '2', 'utf8');
  writeTest.write('e', 3, 'utf8');
//  writeTest.write('j', 'utf8', 4);
  assert.equal(writeTest.toString(), 'nbdees',
    'buffer write API alternative syntax works');
};

exports.testBufferWriteEncoding = function (assert) {

  // Node #1210 Test UTF-8 string includes null character
  var buf = new Buffer('\0');
  assert.equal(buf.length, 1);
  buf = new Buffer('\0\0');
  assert.equal(buf.length, 2);

  buf = new Buffer(2);
  var written = buf.write(''); // 0byte
  assert.equal(written, 0);
  written = buf.write('\0'); // 1byte (v8 adds null terminator)
  assert.equal(written, 1);
  written = buf.write('a\0'); // 1byte * 2
  assert.equal(written, 2);
  // TODO, these tests write 0, possibly due to character encoding
/*
  written = buf.write('あ'); // 3bytes
  assert.equal(written, 0);
  written = buf.write('\0あ'); // 1byte + 3bytes
  assert.equal(written, 1);
*/
  written = buf.write('\0\0あ'); // 1byte * 2 + 3bytes
  buf = new Buffer(10);
  written = buf.write('あいう'); // 3bytes * 3 (v8 adds null terminator)
  assert.equal(written, 9);
  written = buf.write('あいう\0'); // 3bytes * 3 + 1byte
  assert.equal(written, 10);
};

exports.testBufferWriteWithMaxLength = function (assert) {
  // Node #243 Test write() with maxLength
  var buf = new Buffer(4);
  buf.fill(0xFF);
  var written = buf.write('abcd', 1, 2, 'utf8');
  assert.equal(written, 2);
  assert.equal(buf[0], 0xFF);
  assert.equal(buf[1], 0x61);
  assert.equal(buf[2], 0x62);
  assert.equal(buf[3], 0xFF);

  buf.fill(0xFF);
  written = buf.write('abcd', 1, 4);
  assert.equal(written, 3);
  assert.equal(buf[0], 0xFF);
  assert.equal(buf[1], 0x61);
  assert.equal(buf[2], 0x62);
  assert.equal(buf[3], 0x63);

  buf.fill(0xFF);
  // Ignore legacy API
  /*
     written = buf.write('abcd', 'utf8', 1, 2); // legacy style
     console.log(buf);
     assert.equal(written, 2);
     assert.equal(buf[0], 0xFF);
     assert.equal(buf[1], 0x61);
     assert.equal(buf[2], 0x62);
     assert.equal(buf[3], 0xFF);
     */
};

exports.testBufferSlice = function (assert) {
  var asciiString = 'hello world';
  var offset = 100;
  var b = Buffer(1024);
  b.fill(0);

  for (var i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }
  var asciiSlice = b.toString('utf8', 0, asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var written = b.write(asciiString, offset, 'utf8');
  assert.equal(asciiString.length, written);
  asciiSlice = b.toString('utf8', offset, offset + asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var sliceA = b.slice(offset, offset + asciiString.length);
  var sliceB = b.slice(offset, offset + asciiString.length);
  compareBuffers(assert, sliceA, sliceB,
    'slicing is idempotent');

  let sliceTest = true;
  for (var j = 0; j < 100; j++) {
    var slice = b.slice(100, 150);
    if (50 !== slice.length)
      sliceTest = false;
    for (var i = 0; i < 50; i++) {
      if (b[100 + i] !== slice[i])
        sliceTest = false;
    }
  }
  assert.ok(sliceTest, 'massive slice runs do not affect buffer');

  // Single argument slice
  let testBuf = new Buffer('abcde');
  assert.equal('bcde', testBuf.slice(1).toString(), 'single argument slice');

  // slice(0,0).length === 0
  assert.equal(0, Buffer('hello').slice(0, 0).length, 'slice(0,0) === 0');

  var buf = new Buffer('0123456789');
  assert.equal(buf.slice(-10, 10), '0123456789', 'buffer slice range correct');
  assert.equal(buf.slice(-20, 10), '0123456789', 'buffer slice range correct');
  assert.equal(buf.slice(-20, -10), '', 'buffer slice range correct');
  assert.equal(buf.slice(0, -1), '012345678', 'buffer slice range correct');
  assert.equal(buf.slice(2, -2), '234567', 'buffer slice range correct');
  assert.equal(buf.slice(0, 65536), '0123456789', 'buffer slice range correct');
  assert.equal(buf.slice(65536, 0), '', 'buffer slice range correct');

  sliceTest = true;
  for (var i = 0, s = buf.toString(); i < buf.length; ++i) {
    if (buf.slice(-i) != s.slice(-i)) sliceTest = false;
    if (buf.slice(0, -i) != s.slice(0, -i)) sliceTest = false;
  }
  assert.ok(sliceTest, 'buffer.slice should be consistent');

  // Make sure modifying a sliced buffer, affects original and vice versa
  b.fill(0);
  let sliced = b.slice(0, 10);
  let babyslice = sliced.slice(0, 5);

  for (let i = 0; i < sliced.length; i++)
    sliced[i] = 'jetpack'.charAt(i);

  compareBuffers(assert, b, sliced,
    'modifying sliced buffer affects original');

  compareBuffers(assert, b, babyslice,
    'modifying sliced buffer affects child-sliced buffer');

  for (let i = 0; i < sliced.length; i++)
    b[i] = 'odinmonkey'.charAt(i);

  compareBuffers(assert, b, sliced,
    'modifying original buffer affects sliced');

  compareBuffers(assert, b, babyslice,
    'modifying original buffer affects grandchild sliced buffer');
};

exports.testSlicingParents = function (assert) {
  let root = Buffer(5);
  let child = root.slice(0, 4);
  let grandchild = child.slice(0, 3);

  assert.equal(root.parent, undefined, 'a new buffer should not have a parent');

  // make sure a zero length slice doesn't set the .parent attribute
  assert.equal(root.slice(0,0).parent, undefined,
    '0-length slice should not have a parent');

  assert.equal(child.parent, root,
    'a valid slice\'s parent should be the original buffer (child)');

  assert.equal(grandchild.parent, root,
    'a valid slice\'s parent should be the original buffer (grandchild)');
};

exports.testIsBuffer = function (assert) {
  let buffer = new Buffer('content', 'utf8');
  assert.ok(Buffer.isBuffer(buffer), 'isBuffer truthy on buffers');
  assert.ok(!Buffer.isBuffer({}), 'isBuffer falsy on objects');
  assert.ok(!Buffer.isBuffer(new Uint8Array()),
    'isBuffer falsy on Uint8Array');
  assert.ok(Buffer.isBuffer(buffer.slice(0)), 'Buffer#slice should be a new buffer');
};

exports.testBufferConcat = function (assert) {
  let zero = [];
  let one = [ new Buffer('asdf') ];
  let long = [];
  for (let i = 0; i < 10; i++) long.push(new Buffer('asdf'));

  let flatZero = Buffer.concat(zero);
  let flatOne = Buffer.concat(one);
  let flatLong = Buffer.concat(long);
  let flatLongLen = Buffer.concat(long, 40);

  assert.equal(flatZero.length, 0);
  assert.equal(flatOne.toString(), 'asdf');
  assert.equal(flatOne, one[0]);
  assert.equal(flatLong.toString(), (new Array(10+1).join('asdf')));
  assert.equal(flatLongLen.toString(), (new Array(10+1).join('asdf')));
};

exports.testBufferByteLength = function (assert) {
  let str = '\u00bd + \u00bc = \u00be';
  assert.equal(Buffer.byteLength(str), 12,
    'correct byteLength of string');

  assert.equal(14, Buffer.byteLength('Il était tué'));
  assert.equal(14, Buffer.byteLength('Il était tué', 'utf8'));
  // We do not currently support these encodings
  /*
  ['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach(function(encoding) {
  assert.equal(24, Buffer.byteLength('Il était tué', encoding));
  });
  assert.equal(12, Buffer.byteLength('Il était tué', 'ascii'));
  assert.equal(12, Buffer.byteLength('Il était tué', 'binary'));
  */
};

exports.testTextEncoderDecoder = function (assert) {
  assert.ok(TextEncoder, 'TextEncoder exists');
  assert.ok(TextDecoder, 'TextDecoder exists');
};

exports.testOverflowedBuffers = function (assert) {
  assert.throws(function() {
    new Buffer(0xFFFFFFFF);
  }, RangeError, 'correctly throws buffer overflow');

  assert.throws(function() {
    new Buffer(0xFFFFFFFFF);
  }, RangeError, 'correctly throws buffer overflow');

  assert.throws(function() {
    var buf = new Buffer(8);
    buf.readFloatLE(0xffffffff);
  }, RangeError, 'correctly throws buffer overflow with readFloatLE');

  assert.throws(function() {
    var buf = new Buffer(8);
    buf.writeFloatLE(0.0, 0xffffffff);
  }, RangeError, 'correctly throws buffer overflow with writeFloatLE');

  //ensure negative values can't get past offset
  assert.throws(function() {
    var buf = new Buffer(8);
    buf.readFloatLE(-1);
  }, RangeError, 'correctly throws with readFloatLE negative values');

  assert.throws(function() {
    var buf = new Buffer(8);
    buf.writeFloatLE(0.0, -1);
  }, RangeError, 'correctly throws with writeFloatLE with negative values');

  assert.throws(function() {
    var buf = new Buffer(8);
    buf.readFloatLE(-1);
  }, RangeError, 'correctly throws with readFloatLE with negative values');
};

exports.testReadWriteDataTypeErrors = function (assert) {
  var buf = new Buffer(0);
  assert.throws(function() { buf.readUInt8(0); }, RangeError,
    'readUInt8(0) throws');
  assert.throws(function() { buf.readInt8(0); }, RangeError,
    'readInt8(0) throws');

  [16, 32].forEach(function(bits) {
    var buf = new Buffer(bits / 8 - 1);
    assert.throws(function() { buf['readUInt' + bits + 'BE'](0); },
      RangeError,
      'readUInt' + bits + 'BE');

    assert.throws(function() { buf['readUInt' + bits + 'LE'](0); },
      RangeError,
      'readUInt' + bits + 'LE');

    assert.throws(function() { buf['readInt' + bits + 'BE'](0); },
      RangeError,
      'readInt' + bits + 'BE()');

    assert.throws(function() { buf['readInt' + bits + 'LE'](0); },
      RangeError,
      'readInt' + bits + 'LE()');
  });
};

safeMerge(exports, require('./buffers/test-write-types'));
safeMerge(exports, require('./buffers/test-read-types'));

function compareBuffers (assert, buf1, buf2, message) {
  let status = true;
  for (let i = 0; i < Math.min(buf1.length, buf2.length); i++) {
    if (buf1[i] !== buf2[i])
      status = false;
  }
  assert.ok(status, 'buffer successfully copied: ' + message);
}
require('sdk/test').run(exports);
