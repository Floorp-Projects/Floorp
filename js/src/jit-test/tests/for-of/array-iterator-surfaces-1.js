// Superficial tests of the Array.prototype[@@iterator] builtin function and its workalikes.

load(libdir + "iteration.js");

var constructors = [Array, String, Uint8Array, Uint8ClampedArray];
for (var c of constructors) {
    assertEq(c.prototype[std_iterator].length, 0);
    var desc = Object.getOwnPropertyDescriptor(c.prototype, std_iterator);
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);
}
