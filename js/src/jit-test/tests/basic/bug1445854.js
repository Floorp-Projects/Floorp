load(libdir + "asserts.js");
if (typeof ReadableStream == "function") {
    assertThrowsInstanceOf(() => ReadableStream.prototype.tee(),
                           TypeError);
}
