for (var constructor of anyTypedArrayConstructors) {
    var receiver = {};

    var ta = new constructor(1);
    assertEq(Reflect.set(ta, 0, 47, receiver), true);
    assertEq(ta[0], 0);
    assertEq(receiver[0], 47);

    // Out-of-bounds
    assertEq(Reflect.set(ta, 10, 47, receiver), true);
    assertEq(ta[10], undefined);
    assertEq(receiver[10], 47);

    // Detached
    if (typeof detachArrayBuffer === "function" &&
        !isSharedConstructor(constructor))
    {
        detachArrayBuffer(ta.buffer)

        assertEq(ta[0], undefined);
        assertEq(Reflect.set(ta, 0, 42, receiver), true);
        assertEq(ta[0], undefined);
        assertEq(receiver[0], 42);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
