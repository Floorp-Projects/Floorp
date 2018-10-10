// |jit-test| skip-if: !this.SharedArrayBuffer

// SharedArrayBuffer.prototype.byteLength

load(libdir + "asserts.js");

let buffer = new SharedArrayBuffer(137);
assertEq(buffer.byteLength, 137);
