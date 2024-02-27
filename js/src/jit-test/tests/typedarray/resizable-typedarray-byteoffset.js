// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

function testResizableArrayBufferAutoLength() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new Int8Array(ab);
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
    let ta = new Int8Array(ab, 1);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteOffset, 1);

      ab.resize(i + 1);
      assertEq(ta.byteOffset, 1);

      ab.resize(i - 1);
      assertEq(ta.byteOffset, i > 1 ? 1 : 0);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLengthNonZeroOffset();

function testResizableArrayBufferNonZeroOffset() {
  for (let i = 2; i < 4 + 2; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new Int8Array(ab, 1, 1);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteOffset, 1);

      ab.resize(i + 1);
      assertEq(ta.byteOffset, 1);

      ab.resize(i - 1);
      assertEq(ta.byteOffset, i > 2 ? 1 : 0);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferNonZeroOffset();
