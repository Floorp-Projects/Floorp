// SharedArrayBuffer.prototype.byteLength

if (!this.SharedArrayBuffer)
    quit(0);

load(libdir + "asserts.js");

let buffer = new SharedArrayBuffer(137);
assertEq(buffer.byteLength, 137);
