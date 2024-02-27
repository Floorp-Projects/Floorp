// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer

function testResizableArrayBufferAutoLength() {
  for (let i = 0; i < 4; ++i) {
    let sab = new SharedArrayBuffer(i, {maxByteLength: i + 100});
    let ta = new DataView(sab);
    for (let j = 0; j < 100; ++j) {
      assertEq(ta.byteOffset, 0);

      sab.grow(i + j + 1);
      assertEq(ta.byteOffset, 0);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLength();

function testResizableArrayBufferAutoLengthNonZeroOffset() {
  for (let i = 1; i < 4 + 1; ++i) {
    let sab = new SharedArrayBuffer(i + 1, {maxByteLength: i + 100 + 1});
    let ta = new DataView(sab, 1);
    for (let j = 0; j < 100; ++j) {
      assertEq(ta.byteOffset, 1);

      sab.grow(i + j + 2);
      assertEq(ta.byteOffset, 1);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLengthNonZeroOffset();

function testResizableArrayBufferNonZeroOffset() {
  for (let i = 2; i < 4 + 2; ++i) {
    let sab = new SharedArrayBuffer(i + 2, {maxByteLength: i + 100 + 2});
    let ta = new DataView(sab, 1, 1);
    for (let j = 0; j < 100; ++j) {
      assertEq(ta.byteOffset, 1);

      sab.grow(i + j + 3);
      assertEq(ta.byteOffset, 1);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferNonZeroOffset();
