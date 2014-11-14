// Iterating over the argument to WeakMap can throw. The exception is
// propagated.

load(libdir + "asserts.js");

function data() {
    yield [{}, "XR22/Z"];
    yield [{}, "23D-BN"];
    throw "oops";
}

var it = data();
assertThrowsValue(() => new WeakMap(it), "oops");

function* data2() {
    yield [{}, "XR22/Z"];
    yield [{}, "23D-BN"];
    throw "oops";
}

var it2 = data2();
assertThrowsValue(() => new WeakMap(it2), "oops");
