for (var constructor of anyTypedArrayConstructors) {
    assertThrowsInstanceOf(() => constructor(), TypeError);
    assertThrowsInstanceOf(() => constructor(1), TypeError);
    assertThrowsInstanceOf(() => constructor.call(null), TypeError);
    assertThrowsInstanceOf(() => constructor.apply(null, []), TypeError);
    assertThrowsInstanceOf(() => Reflect.apply(constructor, null, []), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
