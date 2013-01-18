// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-0428dbdf3d58-linux
// Flags:
//
o1 = Float32Array().buffer
o2 = ArrayBuffer.prototype
o3 = Uint32Array().buffer
for (i = 0; i < 2; i++) {
    for (var x in o2) {
        o3.__defineGetter__("", function() {})
    }
    o2.__defineGetter__("", function() {})
    o1[
    x]
    o1.__proto__ = o3
}
