for (var constructor of anyTypedArrayConstructors) {
    // Check superficial features of %TypeArray%.from.
    var desc = Object.getOwnPropertyDescriptor(constructor.__proto__, "from");
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);
    assertEq(constructor.from.length, 1);
    assertThrowsInstanceOf(() => new constructor.from(), TypeError);  // not a constructor
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
