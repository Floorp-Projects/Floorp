// for-of throws if the target is an ArrayBuffer.

load(libdir + "asserts.js");
assertThrowsInstanceOf(function () {
    for (var v of Int8Array([0, 1, 2, 3]).buffer)
        throw "FAIL";
}, TypeError);
