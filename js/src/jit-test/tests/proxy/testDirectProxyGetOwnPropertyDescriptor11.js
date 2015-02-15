// Bug 1133294 - Object.getOwnPropertyDescriptor should never return an incomplete descriptor.

load(libdir + "asserts.js");

var p = new Proxy({}, {
    getOwnPropertyDescriptor() { return {configurable: true}; }
});
var desc = Object.getOwnPropertyDescriptor(p, "x");
assertDeepEq(desc, {
    configurable: true,
    enumerable: false,
    value: undefined,
    writable: false
});
