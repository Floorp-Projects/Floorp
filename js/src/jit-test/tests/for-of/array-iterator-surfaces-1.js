// Superficial tests of the Array.prototype.iterator builtin function and its workalikes.

var constructors = [Array, String, Uint8Array, Uint8ClampedArray];
for (var c of constructors) {
    assertEq(c.prototype.iterator.length, 0);
    var desc = Object.getOwnPropertyDescriptor(c.prototype, "iterator");
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);
}
