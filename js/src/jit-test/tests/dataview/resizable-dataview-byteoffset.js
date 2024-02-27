// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

load(libdir + "asserts.js");

function testResizableArrayBufferAutoLength() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new DataView(ab);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteOffset, 0);

      ab.resize(i + 1);
      assertEq(ta.byteOffset, 0);

      if (i > 0) {
        ab.resize(i - 1);
        assertEq(ta.byteOffset, 0);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLength();

function testResizableArrayBufferAutoLengthNonZeroOffset() {
  for (let i = 1; i < 4 + 1; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new DataView(ab, 1);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteOffset, 1);

      ab.resize(i + 1);
      assertEq(ta.byteOffset, 1);

      ab.resize(i - 1);
      if (i > 1) {
        assertEq(ta.byteOffset, 1);
      } else {
        assertThrowsInstanceOf(() => ta.byteOffset, TypeError);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLengthNonZeroOffset();

function testResizableArrayBufferNonZeroOffset() {
  for (let i = 2; i < 4 + 2; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new DataView(ab, 1, 1);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteOffset, 1);

      ab.resize(i + 1);
      assertEq(ta.byteOffset, 1);

      ab.resize(i - 1);
      if (i > 2) {
        assertEq(ta.byteOffset, 1);
      } else {
        assertThrowsInstanceOf(() => ta.byteOffset, TypeError);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferNonZeroOffset();
