// If Array.of tries to overwrite a non-configurable property, it throws a TypeError.

load(libdir + "asserts.js");

function C() {
    Object.defineProperty(this, 0, {value: "v", configurable: false});
}
assertThrowsInstanceOf(() => Array.of.call(C, 1, 2, 3), TypeError);
