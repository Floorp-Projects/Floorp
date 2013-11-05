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

const { Buffer } = require('sdk/io/buffer');

exports.testReadDouble = helper('readDoubleLE/readDoubleBE', function (assert) {
  var buffer = new Buffer(8);

  buffer[0] = 0x55;
  buffer[1] = 0x55;
  buffer[2] = 0x55;
  buffer[3] = 0x55;
  buffer[4] = 0x55;
  buffer[5] = 0x55;
  buffer[6] = 0xd5;
  buffer[7] = 0x3f;
  assert.equal(1.1945305291680097e+103, buffer.readDoubleBE(0));
  assert.equal(0.3333333333333333, buffer.readDoubleLE(0));

  buffer[0] = 1;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 0;
  buffer[6] = 0xf0;
  buffer[7] = 0x3f;
  assert.equal(7.291122019655968e-304, buffer.readDoubleBE(0));
  assert.equal(1.0000000000000002, buffer.readDoubleLE(0));

  buffer[0] = 2;
  assert.equal(4.778309726801735e-299, buffer.readDoubleBE(0));
  assert.equal(1.0000000000000004, buffer.readDoubleLE(0));

  buffer[0] = 1;
  buffer[6] = 0;
  buffer[7] = 0;
  assert.equal(7.291122019556398e-304, buffer.readDoubleBE(0));
  assert.equal(5e-324, buffer.readDoubleLE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0xff;
  buffer[3] = 0xff;
  buffer[4] = 0xff;
  buffer[5] = 0xff;
  buffer[6] = 0x0f;
  buffer[7] = 0x00;
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.equal(2.225073858507201e-308, buffer.readDoubleLE(0));

  buffer[6] = 0xef;
  buffer[7] = 0x7f;
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.equal(1.7976931348623157e+308, buffer.readDoubleLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 0;
  buffer[6] = 0xf0;
  buffer[7] = 0x3f;
  assert.equal(3.03865e-319, buffer.readDoubleBE(0));
  assert.equal(1, buffer.readDoubleLE(0));

  buffer[6] = 0;
  buffer[7] = 0x40;
  assert.equal(3.16e-322, buffer.readDoubleBE(0));
  assert.equal(2, buffer.readDoubleLE(0));

  buffer[7] = 0xc0;
  assert.equal(9.5e-322, buffer.readDoubleBE(0));
  assert.equal(-2, buffer.readDoubleLE(0));

  buffer[6] = 0x10;
  buffer[7] = 0;
  assert.equal(2.0237e-320, buffer.readDoubleBE(0));
  assert.equal(2.2250738585072014e-308, buffer.readDoubleLE(0));

  buffer[6] = 0;
  assert.equal(0, buffer.readDoubleBE(0));
  assert.equal(0, buffer.readDoubleLE(0));
  assert.equal(false, 1 / buffer.readDoubleLE(0) < 0);

  buffer[7] = 0x80;
  assert.equal(6.3e-322, buffer.readDoubleBE(0));
  assert.equal(0, buffer.readDoubleLE(0));
  assert.equal(true, 1 / buffer.readDoubleLE(0) < 0);

  buffer[6] = 0xf0;
  buffer[7] = 0x7f;
  assert.equal(3.0418e-319, buffer.readDoubleBE(0));
  assert.equal(Infinity, buffer.readDoubleLE(0));

  buffer[6] = 0xf0;
  buffer[7] = 0xff;
  assert.equal(3.04814e-319, buffer.readDoubleBE(0));
  assert.equal(-Infinity, buffer.readDoubleLE(0));
});


exports.testReadFloat = helper('readFloatLE/readFloatBE', function (assert) {
  var buffer = new Buffer(4);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x3f;
  assert.equal(4.600602988224807e-41, buffer.readFloatBE(0));
  assert.equal(1, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0xc0;
  assert.equal(2.6904930515036488e-43, buffer.readFloatBE(0));
  assert.equal(-2, buffer.readFloatLE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0x7f;
  buffer[3] = 0x7f;
  assert.ok(isNaN(buffer.readFloatBE(0)));
  assert.equal(3.4028234663852886e+38, buffer.readFloatLE(0));

  buffer[0] = 0xab;
  buffer[1] = 0xaa;
  buffer[2] = 0xaa;
  buffer[3] = 0x3e;
  assert.equal(-1.2126478207002966e-12, buffer.readFloatBE(0));
  assert.equal(0.3333333432674408, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  assert.equal(0, buffer.readFloatBE(0));
  assert.equal(0, buffer.readFloatLE(0));
  assert.equal(false, 1 / buffer.readFloatLE(0) < 0);

  buffer[3] = 0x80;
  assert.equal(1.793662034335766e-43, buffer.readFloatBE(0));
  assert.equal(0, buffer.readFloatLE(0));
  assert.equal(true, 1 / buffer.readFloatLE(0) < 0);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x7f;
  assert.equal(4.609571298396486e-41, buffer.readFloatBE(0));
  assert.equal(Infinity, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0xff;
  assert.equal(4.627507918739843e-41, buffer.readFloatBE(0));
  assert.equal(-Infinity, buffer.readFloatLE(0));
});


exports.testReadInt8 = helper('readInt8', function (assert) {
  var data = new Buffer(4);

  data[0] = 0x23;
  assert.equal(0x23, data.readInt8(0));

  data[0] = 0xff;
  assert.equal(-1, data.readInt8(0));

  data[0] = 0x87;
  data[1] = 0xab;
  data[2] = 0x7c;
  data[3] = 0xef;
  assert.equal(-121, data.readInt8(0));
  assert.equal(-85, data.readInt8(1));
  assert.equal(124, data.readInt8(2));
  assert.equal(-17, data.readInt8(3));
});


exports.testReadInt16 = helper('readInt16BE/readInt16LE', function (assert) {
  var buffer = new Buffer(6);

  buffer[0] = 0x16;
  buffer[1] = 0x79;
  assert.equal(0x1679, buffer.readInt16BE(0));
  assert.equal(0x7916, buffer.readInt16LE(0));

  buffer[0] = 0xff;
  buffer[1] = 0x80;
  assert.equal(-128, buffer.readInt16BE(0));
  assert.equal(-32513, buffer.readInt16LE(0));

  /* test offset with weenix */
  buffer[0] = 0x77;
  buffer[1] = 0x65;
  buffer[2] = 0x65;
  buffer[3] = 0x6e;
  buffer[4] = 0x69;
  buffer[5] = 0x78;
  assert.equal(0x7765, buffer.readInt16BE(0));
  assert.equal(0x6565, buffer.readInt16BE(1));
  assert.equal(0x656e, buffer.readInt16BE(2));
  assert.equal(0x6e69, buffer.readInt16BE(3));
  assert.equal(0x6978, buffer.readInt16BE(4));
  assert.equal(0x6577, buffer.readInt16LE(0));
  assert.equal(0x6565, buffer.readInt16LE(1));
  assert.equal(0x6e65, buffer.readInt16LE(2));
  assert.equal(0x696e, buffer.readInt16LE(3));
  assert.equal(0x7869, buffer.readInt16LE(4));
});


exports.testReadInt32 = helper('readInt32BE/readInt32LE', function (assert) {
  var buffer = new Buffer(6);

  buffer[0] = 0x43;
  buffer[1] = 0x53;
  buffer[2] = 0x16;
  buffer[3] = 0x79;
  assert.equal(0x43531679, buffer.readInt32BE(0));
  assert.equal(0x79165343, buffer.readInt32LE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xfe;
  buffer[2] = 0xef;
  buffer[3] = 0xfa;
  assert.equal(-69638, buffer.readInt32BE(0));
  assert.equal(-84934913, buffer.readInt32LE(0));

  buffer[0] = 0x42;
  buffer[1] = 0xc3;
  buffer[2] = 0x95;
  buffer[3] = 0xa9;
  buffer[4] = 0x36;
  buffer[5] = 0x17;
  assert.equal(0x42c395a9, buffer.readInt32BE(0));
  assert.equal(-1013601994, buffer.readInt32BE(1));
  assert.equal(-1784072681, buffer.readInt32BE(2));
  assert.equal(-1449802942, buffer.readInt32LE(0));
  assert.equal(917083587, buffer.readInt32LE(1));
  assert.equal(389458325, buffer.readInt32LE(2));
});


/*
 * We need to check the following things:
 *  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
 *  - Correctly resolving little endian (doesn't mean anything for 8 bit)
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
exports.testReadUInt8 = helper('readUInt8', function (assert) {
  var data = new Buffer(4);

  data[0] = 23;
  data[1] = 23;
  data[2] = 23;
  data[3] = 23;
  assert.equal(23, data.readUInt8(0));
  assert.equal(23, data.readUInt8(1));
  assert.equal(23, data.readUInt8(2));
  assert.equal(23, data.readUInt8(3));

  data[0] = 255; /* If it became a signed int, would be -1 */
  assert.equal(255, data.readUInt8(0));
});


/*
 * Test 16 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly resolving little endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
exports.testReadUInt16 = helper('readUInt16LE/readUInt16BE', function (assert) {
  var data = new Buffer(4);

  data[0] = 0;
  data[1] = 0x23;
  data[2] = 0x42;
  data[3] = 0x3f;
  assert.equal(0x23, data.readUInt16BE(0));
  assert.equal(0x2342, data.readUInt16BE(1));
  assert.equal(0x423f, data.readUInt16BE(2));
  assert.equal(0x2300, data.readUInt16LE(0));
  assert.equal(0x4223, data.readUInt16LE(1));
  assert.equal(0x3f42, data.readUInt16LE(2));

  data[0] = 0xfe;
  data[1] = 0xfe;
  assert.equal(0xfefe, data.readUInt16BE(0));
  assert.equal(0xfefe, data.readUInt16LE(0));
});


/*
 * Test 32 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
exports.testReadUInt32 = helper('readUInt32LE/readUInt32BE', function (assert) {
  var data = new Buffer(8);

  data[0] = 0x32;
  data[1] = 0x65;
  data[2] = 0x42;
  data[3] = 0x56;
  data[4] = 0x23;
  data[5] = 0xff;
  assert.equal(0x32654256, data.readUInt32BE(0));
  assert.equal(0x65425623, data.readUInt32BE(1));
  assert.equal(0x425623ff, data.readUInt32BE(2));
  assert.equal(0x56426532, data.readUInt32LE(0));
  assert.equal(0x23564265, data.readUInt32LE(1));
  assert.equal(0xff235642, data.readUInt32LE(2));
});

function helper (description, fn) {
  let bulkAssert = {
    equal: function (a, b) {
      if (a !== b) throw new Error('Error found in ' + description);
    },
    ok: function (value) {
      if (!value) throw new Error('Error found in ' + description);
    },
    throws: function (shouldThrow) {
      let didItThrow = false;
      try {
        shouldThrow();
      } catch (e) {
        didItThrow = e;
      }
      if (!didItThrow)
        throw new Error('Error found in ' + description + ': ' + shouldThrow + ' should have thrown');
    }
  };
  return function (assert) {
    fn(bulkAssert);
    // If we get here, no errors thrown
    assert.pass('All tests passed for ' + description);
  };
}
