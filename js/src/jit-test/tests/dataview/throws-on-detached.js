// TypeError is thrown when the underlying ArrayBuffer is detached.

function testByteOffset() {
  var ab = new ArrayBuffer(10);
  var dv = new DataView(ab, 4, 0);

  var q = 0;
  var error;
  try {
    for (var i = 0; i <= 200; ++i) {
      if (i === 200) {
        detachArrayBuffer(ab);
      }
      q += dv.byteOffset;
    }
  } catch (e) {
    error = e;
  }
  assertEq(q, 4 * 200);
  assertEq(error instanceof TypeError, true);
}
testByteOffset();

function testByteLength() {
  var ab = new ArrayBuffer(10);
  var dv = new DataView(ab, 4, 6);

  var q = 0;
  var error;
  try {
    for (var i = 0; i <= 200; ++i) {
      if (i === 200) {
        detachArrayBuffer(ab);
      }
      q += dv.byteLength;
    }
  } catch (e) {
    error = e;
  }
  assertEq(q, 6 * 200);
  assertEq(error instanceof TypeError, true);
}
testByteLength();
