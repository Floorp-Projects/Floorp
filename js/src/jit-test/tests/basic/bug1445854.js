// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
load(libdir + "asserts.js");
assertThrowsInstanceOf(() => ReadableStream.prototype.tee(),
    TypeError);
