// Map(x) throws if x is not iterable (unless x is undefined).

load(libdir + "asserts.js");
var nonIterables = [null, true, 1, -0, 3.14, NaN, "", "xyzzy", {}, Math, this];
for (let k of nonIterables)
    assertThrowsInstanceOf(function () { Map(k); }, TypeError);
