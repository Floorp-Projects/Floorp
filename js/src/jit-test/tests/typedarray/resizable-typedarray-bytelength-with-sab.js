// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer

function testResizableArrayBuffer() {
  for (let i = 0; i < 4; ++i) {
    let sab = new SharedArrayBuffer(i, {maxByteLength: i + 100});
    let ta = new Int8Array(sab, 0, i);
    for (let j = 0; j < 100; ++j) {
      assertEq(ta.byteLength, i);

      sab.grow(i + j + 1);
      assertEq(ta.byteLength, i);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBuffer();

function testResizableArrayBufferAutoLength() {
  for (let i = 0; i < 4; ++i) {
    let sab = new SharedArrayBuffer(i, {maxByteLength: i + 100});
    let ta = new Int8Array(sab);
    for (let j = 0; j < 100; ++j) {
      assertEq(ta.byteLength, i + j);

      sab.grow(i + j + 1);
      assertEq(ta.byteLength, i + j + 1);
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLength();
