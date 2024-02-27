// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

load(libdir + "asserts.js");

function testResizableArrayBuffer() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new DataView(ab, 0, i);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteLength, i);

      ab.resize(i + 1);
      assertEq(ta.byteLength, i);

      if (i > 0) {
        ab.resize(i - 1);
        assertThrowsInstanceOf(() => ta.byteLength, TypeError);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBuffer();

function testResizableArrayBufferAutoLength() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i, {maxByteLength: i + 1});
    let ta = new DataView(ab);
    for (let j = 0; j < 100; ++j) {
      ab.resize(i);
      assertEq(ta.byteLength, i);

      ab.resize(i + 1);
      assertEq(ta.byteLength, i + 1);

      if (i > 0) {
        ab.resize(i - 1);
        assertEq(ta.byteLength, i - 1);
      }
    }
  }
}
for (let i = 0; i < 2; ++i) testResizableArrayBufferAutoLength();
