// WeakMap(x) throws if x is not iterable (unless x is undefined).

load(libdir + "asserts.js");
// FIXME: bug 1092538
// `new WeapMap(null)` should not throw.
var nonIterables = [null, true, 1, -0, 3.14, NaN, {}, Math, this];
for (let k of nonIterables)
    assertThrowsInstanceOf(function () { new WeakMap(k); }, TypeError);
