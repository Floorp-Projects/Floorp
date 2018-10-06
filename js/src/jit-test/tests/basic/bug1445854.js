load(libdir + "asserts.js");
if (typeof ReadableStream == "function") {
    assertThrowsInstanceOf(TypeError, () => ReadableStream.prototype.tee());
}
