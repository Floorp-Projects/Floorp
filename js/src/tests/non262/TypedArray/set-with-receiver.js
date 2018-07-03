for (var constructor of anyTypedArrayConstructors) {
    var receiver = new Proxy({}, {
        getOwnPropertyDescriptor(p) {
            throw "fail";
        },

        defineProperty() {
            throw "fail";
        }
    });

    var ta = new constructor(1);
    assertEq(Reflect.set(ta, 0, 47, receiver), true);
    assertEq(ta[0], 47);

    // Out-of-bounds
    assertEq(Reflect.set(ta, 10, 47, receiver), false);
    assertEq(ta[10], undefined);

    // Detached
    if (typeof detachArrayBuffer === "function" &&
        !isSharedConstructor(constructor))
    {
        detachArrayBuffer(ta.buffer)

        assertEq(ta[0], undefined);
        assertEq(Reflect.set(ta, 0, 47, receiver), false);
        assertEq(ta[0], undefined);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
