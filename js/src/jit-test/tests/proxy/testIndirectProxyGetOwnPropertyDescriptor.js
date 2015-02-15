// Bug 1133294 - Object.getOwnPropertyDescriptor should never return an incomplete descriptor.

load(libdir + "asserts.js");

var p = Proxy.create({ getOwnPropertyDescriptor() { return {}; } });
var desc = Object.getOwnPropertyDescriptor(p, "x");
assertDeepEq(desc, {
    configurable: false,
    enumerable: false,
    value: undefined,
    writable: false
});
