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

exports.testWriteDouble = helper('writeDoubleBE/writeDoubleLE', function (assert) {
  var buffer = new Buffer(16);

  buffer.writeDoubleBE(2.225073858507201e-308, 0);
  buffer.writeDoubleLE(2.225073858507201e-308, 8);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x0f, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0xff, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);
  assert.equal(0xff, buffer[8]);
  assert.equal(0xff, buffer[9]);
  assert.equal(0xff, buffer[10]);
  assert.equal(0xff, buffer[11]);
  assert.equal(0xff, buffer[12]);
  assert.equal(0xff, buffer[13]);
  assert.equal(0x0f, buffer[14]);
  assert.equal(0x00, buffer[15]);

  buffer.writeDoubleBE(1.0000000000000004, 0);
  buffer.writeDoubleLE(1.0000000000000004, 8);
  assert.equal(0x3f, buffer[0]);
  assert.equal(0xf0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x02, buffer[7]);
  assert.equal(0x02, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xf0, buffer[14]);
  assert.equal(0x3f, buffer[15]);

  buffer.writeDoubleBE(-2, 0);
  buffer.writeDoubleLE(-2, 8);
  assert.equal(0xc0, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0x00, buffer[14]);
  assert.equal(0xc0, buffer[15]);

  buffer.writeDoubleBE(1.7976931348623157e+308, 0);
  buffer.writeDoubleLE(1.7976931348623157e+308, 8);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xef, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0xff, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);
  assert.equal(0xff, buffer[8]);
  assert.equal(0xff, buffer[9]);
  assert.equal(0xff, buffer[10]);
  assert.equal(0xff, buffer[11]);
  assert.equal(0xff, buffer[12]);
  assert.equal(0xff, buffer[13]);
  assert.equal(0xef, buffer[14]);
  assert.equal(0x7f, buffer[15]);

  buffer.writeDoubleBE(0 * -1, 0);
  buffer.writeDoubleLE(0 * -1, 8);
  assert.equal(0x80, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0x00, buffer[14]);
  assert.equal(0x80, buffer[15]);

  buffer.writeDoubleBE(Infinity, 0);
  buffer.writeDoubleLE(Infinity, 8);
  assert.equal(0x7F, buffer[0]);
  assert.equal(0xF0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xF0, buffer[14]);
  assert.equal(0x7F, buffer[15]);
  assert.equal(Infinity, buffer.readDoubleBE(0));
  assert.equal(Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(-Infinity, 0);
  buffer.writeDoubleLE(-Infinity, 8);
  assert.equal(0xFF, buffer[0]);
  assert.equal(0xF0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xF0, buffer[14]);
  assert.equal(0xFF, buffer[15]);
  assert.equal(-Infinity, buffer.readDoubleBE(0));
  assert.equal(-Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(NaN, 0);
  buffer.writeDoubleLE(NaN, 8);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert.ok(0x7F === buffer[0] || 0xFF === buffer[0]);
  assert.equal(0xF8, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xF8, buffer[14]);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert.ok(0x7F === buffer[15] || 0xFF === buffer[15]);
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.ok(isNaN(buffer.readDoubleLE(8)));
});

exports.testWriteFloat = helper('writeFloatBE/writeFloatLE', function (assert) {
  var buffer = new Buffer(8);

  buffer.writeFloatBE(1, 0);
  buffer.writeFloatLE(1, 4);
  assert.equal(0x3f, buffer[0]);
  assert.equal(0x80, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x80, buffer[6]);
  assert.equal(0x3f, buffer[7]);

  buffer.writeFloatBE(1 / 3, 0);
  buffer.writeFloatLE(1 / 3, 4);
  assert.equal(0x3e, buffer[0]);
  assert.equal(0xaa, buffer[1]);
  assert.equal(0xaa, buffer[2]);
  assert.equal(0xab, buffer[3]);
  assert.equal(0xab, buffer[4]);
  assert.equal(0xaa, buffer[5]);
  assert.equal(0xaa, buffer[6]);
  assert.equal(0x3e, buffer[7]);

  buffer.writeFloatBE(3.4028234663852886e+38, 0);
  buffer.writeFloatLE(3.4028234663852886e+38, 4);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0x7f, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0xff, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0x7f, buffer[6]);
  assert.equal(0x7f, buffer[7]);

  buffer.writeFloatLE(1.1754943508222875e-38, 0);
  buffer.writeFloatBE(1.1754943508222875e-38, 4);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x80, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x80, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);

  buffer.writeFloatBE(0 * -1, 0);
  buffer.writeFloatLE(0 * -1, 4);
  assert.equal(0x80, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x80, buffer[7]);

  buffer.writeFloatBE(Infinity, 0);
  buffer.writeFloatLE(Infinity, 4);
  assert.equal(0x7F, buffer[0]);
  assert.equal(0x80, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x80, buffer[6]);
  assert.equal(0x7F, buffer[7]);
  assert.equal(Infinity, buffer.readFloatBE(0));
  assert.equal(Infinity, buffer.readFloatLE(4));

  buffer.writeFloatBE(-Infinity, 0);
  buffer.writeFloatLE(-Infinity, 4);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert.ok(0xFF === buffer[0] || 0x7F === buffer[0]);
  assert.equal(0x80, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x80, buffer[6]);
  assert.equal(0xFF, buffer[7]);
  assert.equal(-Infinity, buffer.readFloatBE(0));
  assert.equal(-Infinity, buffer.readFloatLE(4));

  buffer.writeFloatBE(NaN, 0);
  buffer.writeFloatLE(NaN, 4);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert.ok(0x7F === buffer[0] || 0xFF === buffer[0]);
  assert.equal(0xc0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0xc0, buffer[6]);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert.ok(0x7F === buffer[7] || 0xFF === buffer[7]);
  assert.ok(isNaN(buffer.readFloatBE(0)));
  assert.ok(isNaN(buffer.readFloatLE(4)));
});


exports.testWriteInt8 = helper('writeInt8', function (assert) {
  var buffer = new Buffer(2);

  buffer.writeInt8(0x23, 0);
  buffer.writeInt8(-5, 1);

  assert.equal(0x23, buffer[0]);
  assert.equal(0xfb, buffer[1]);

  /* Make sure we handle truncation correctly */
  assert.throws(function() {
    buffer.writeInt8(0xabc, 0);
  });
  assert.throws(function() {
    buffer.writeInt8(0xabc, 0);
  });

  /* Make sure we handle min/max correctly */
  buffer.writeInt8(0x7f, 0);
  buffer.writeInt8(-0x80, 1);

  assert.equal(0x7f, buffer[0]);
  assert.equal(0x80, buffer[1]);
  assert.throws(function() {
    buffer.writeInt8(0x7f + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt8(-0x80 - 1, 0);
  });
});


exports.testWriteInt16 = helper('writeInt16LE/writeInt16BE', function (assert) {
  var buffer = new Buffer(6);

  buffer.writeInt16BE(0x0023, 0);
  buffer.writeInt16LE(0x0023, 2);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x23, buffer[1]);
  assert.equal(0x23, buffer[2]);
  assert.equal(0x00, buffer[3]);

  buffer.writeInt16BE(-5, 0);
  buffer.writeInt16LE(-5, 2);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xfb, buffer[1]);
  assert.equal(0xfb, buffer[2]);
  assert.equal(0xff, buffer[3]);

  buffer.writeInt16BE(-1679, 1);
  buffer.writeInt16LE(-1679, 3);
  assert.equal(0xf9, buffer[1]);
  assert.equal(0x71, buffer[2]);
  assert.equal(0x71, buffer[3]);
  assert.equal(0xf9, buffer[4]);

  /* Make sure we handle min/max correctly */
  buffer.writeInt16BE(0x7fff, 0);
  buffer.writeInt16BE(-0x8000, 2);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0x80, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.throws(function() {
    buffer.writeInt16BE(0x7fff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt16BE(-0x8000 - 1, 0);
  });

  buffer.writeInt16LE(0x7fff, 0);
  buffer.writeInt16LE(-0x8000, 2);
  assert.equal(0xff, buffer[0]);
  assert.equal(0x7f, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x80, buffer[3]);
  assert.throws(function() {
    buffer.writeInt16LE(0x7fff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt16LE(-0x8000 - 1, 0);
  });
});

exports.testWriteInt32 = helper('writeInt32BE/writeInt32LE', function (assert) {
  var buffer = new Buffer(8);

  buffer.writeInt32BE(0x23, 0);
  buffer.writeInt32LE(0x23, 4);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x23, buffer[3]);
  assert.equal(0x23, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);

  buffer.writeInt32BE(-5, 0);
  buffer.writeInt32LE(-5, 4);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xfb, buffer[3]);
  assert.equal(0xfb, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);

  buffer.writeInt32BE(-805306713, 0);
  buffer.writeInt32LE(-805306713, 4);
  assert.equal(0xcf, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xfe, buffer[2]);
  assert.equal(0xa7, buffer[3]);
  assert.equal(0xa7, buffer[4]);
  assert.equal(0xfe, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xcf, buffer[7]);

  /* Make sure we handle min/max correctly */
  buffer.writeInt32BE(0x7fffffff, 0);
  buffer.writeInt32BE(-0x80000000, 4);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0x80, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.throws(function() {
    buffer.writeInt32BE(0x7fffffff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt32BE(-0x80000000 - 1, 0);
  });

  buffer.writeInt32LE(0x7fffffff, 0);
  buffer.writeInt32LE(-0x80000000, 4);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0x7f, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x80, buffer[7]);
  assert.throws(function() {
    buffer.writeInt32LE(0x7fffffff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt32LE(-0x80000000 - 1, 0);
  });
});

/*
 * We need to check the following things:
 *  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
 *  - Correctly resolving little endian (doesn't mean anything for 8 bit)
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
exports.testWriteUInt8 = helper('writeUInt8', function (assert) {
  var data = new Buffer(4);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.equal(23, data[0]);
  assert.equal(23, data[1]);
  assert.equal(23, data[2]);
  assert.equal(23, data[3]);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.equal(23, data[0]);
  assert.equal(23, data[1]);
  assert.equal(23, data[2]);
  assert.equal(23, data[3]);

  data.writeUInt8(255, 0);
  assert.equal(255, data[0]);

  data.writeUInt8(255, 0);
  assert.equal(255, data[0]);
});


exports.testWriteUInt16 = helper('writeUInt16BE/writeUInt16LE', function (assert) {
  var value = 0x2343;
  var data = new Buffer(4);

  data.writeUInt16BE(value, 0);
  assert.equal(0x23, data[0]);
  assert.equal(0x43, data[1]);

  data.writeUInt16BE(value, 1);
  assert.equal(0x23, data[1]);
  assert.equal(0x43, data[2]);

  data.writeUInt16BE(value, 2);
  assert.equal(0x23, data[2]);
  assert.equal(0x43, data[3]);

  data.writeUInt16LE(value, 0);
  assert.equal(0x23, data[1]);
  assert.equal(0x43, data[0]);

  data.writeUInt16LE(value, 1);
  assert.equal(0x23, data[2]);
  assert.equal(0x43, data[1]);

  data.writeUInt16LE(value, 2);
  assert.equal(0x23, data[3]);
  assert.equal(0x43, data[2]);

  value = 0xff80;
  data.writeUInt16LE(value, 0);
  assert.equal(0xff, data[1]);
  assert.equal(0x80, data[0]);

  data.writeUInt16BE(value, 0);
  assert.equal(0xff, data[0]);
  assert.equal(0x80, data[1]);
});


exports.testWriteUInt32 = helper('writeUInt32BE/writeUInt32LE', function (assert) {
  var data = new Buffer(6);
  var value = 0xe7f90a6d;

  data.writeUInt32BE(value, 0);
  assert.equal(0xe7, data[0]);
  assert.equal(0xf9, data[1]);
  assert.equal(0x0a, data[2]);
  assert.equal(0x6d, data[3]);

  data.writeUInt32BE(value, 1);
  assert.equal(0xe7, data[1]);
  assert.equal(0xf9, data[2]);
  assert.equal(0x0a, data[3]);
  assert.equal(0x6d, data[4]);

  data.writeUInt32BE(value, 2);
  assert.equal(0xe7, data[2]);
  assert.equal(0xf9, data[3]);
  assert.equal(0x0a, data[4]);
  assert.equal(0x6d, data[5]);

  data.writeUInt32LE(value, 0);
  assert.equal(0xe7, data[3]);
  assert.equal(0xf9, data[2]);
  assert.equal(0x0a, data[1]);
  assert.equal(0x6d, data[0]);

  data.writeUInt32LE(value, 1);
  assert.equal(0xe7, data[4]);
  assert.equal(0xf9, data[3]);
  assert.equal(0x0a, data[2]);
  assert.equal(0x6d, data[1]);

  data.writeUInt32LE(value, 2);
  assert.equal(0xe7, data[5]);
  assert.equal(0xf9, data[4]);
  assert.equal(0x0a, data[3]);
  assert.equal(0x6d, data[2]);
});

function helper (description, fn) {
  return function (assert) {
    let bulkAssert = {
      equal: function (a, b) {
        if (a !== b) throw new Error('Error found in ' + description);
      },
      ok: function (value) {
        if (!value) throw new Error('Error found in ' + description);
      },
      /*
       * TODO
       * There should be errors when setting outside of the value range
       * of the data type (like writeInt8 with value of 1000), but DataView
       * does not throw; seems to just grab the appropriate max bits.
       * So ignoring this test for now
       */
      throws: function (shouldThrow) {
        assert.pass(description + ': Need to implement error handling for setting buffer values ' +
          'outside of the data types\' range.');
        /*
        let didItThrow = false;
        try {
          shouldThrow();
        } catch (e) {
          didItThrow = e;
        }
        if (!didItThrow)
          throw new Error('Error found in ' + description + ': ' + shouldThrow + ' should have thrown');
        */
      }
    };
    fn(bulkAssert);
    // If we get here, no errors thrown
    assert.pass('All tests passed for ' + description);
  };
}
