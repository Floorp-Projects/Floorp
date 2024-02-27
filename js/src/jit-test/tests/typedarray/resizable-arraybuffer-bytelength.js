// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

function testResizableArrayBuffer() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ab.byteLength, i);

      ab.resize(i + 1);
      assertEq(ab.byteLength, i + 1);

      if (i > 0) {
        ab.resize(i - 1);
        assertEq(ab.byteLength, i - 1);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBuffer();
