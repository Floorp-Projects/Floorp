load(libdir + "asserts.js");

function testNonExtensible() {
    var a = [1, 2, 3, , 5];
    Object.preventExtensions(a);

    // Can change the value.
    Object.defineProperty(a, 1, {value:7, enumerable: true, configurable: true, writable: true});
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptor(a, 1)),
            `{"value":7,"writable":true,"enumerable":true,"configurable":true}`);

    // Can make non-writable, non-configurable, non-enumerable.
    Object.defineProperty(a, 1, {value:9, enumerable: true, configurable: true, writable: false});
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptor(a, 1)),
            `{"value":9,"writable":false,"enumerable":true,"configurable":true}`);
    Object.defineProperty(a, 0, {value:4, enumerable: true, configurable: false, writable: true});
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptor(a, 0)),
             `{"value":4,"writable":true,"enumerable":true,"configurable":false}`);
    Object.defineProperty(a, 2, {value:3, enumerable: false, configurable: true, writable: true});
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptor(a, 2)),
             `{"value":3,"writable":true,"enumerable":false,"configurable":true}`);

    // Turn into an accessor.
    Object.defineProperty(a, 4, {get:() => -2, enumerable: true, configurable: true});

    // Can't add new properties.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 3,
                                                       {value:4, enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);
    assertThrowsInstanceOf(() => Object.defineProperty(a, 10,
                                                       {value:4,
                                                        enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);

    assertEq(a.toString(), "4,9,3,,-2");
}
for (var i = 0; i < 15; i++)
    testNonExtensible();

function testSealed() {
    var a = [1, 2, 3, , 5];
    Object.seal(a);

    // Can change the value.
    Object.defineProperty(a, 1, {value:7, enumerable: true, configurable: false, writable: true});
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptor(a, 1)),
             `{"value":7,"writable":true,"enumerable":true,"configurable":false}`);

    // Can make non-writable.
    Object.defineProperty(a, 0, {value:4, enumerable: true, configurable: false, writable: false});

    // Can't make configurable, non-enumerable.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 2,
                                                       {value:7,
                                                        enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);
    assertThrowsInstanceOf(() => Object.defineProperty(a, 2,
                                                       {value:7,
                                                        enumerable: false,
                                                        configurable: false,
                                                        writable: true}),
                           TypeError);

    // Can't turn into an accessor.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 4, {get:() => -2,
                                                              enumerable: true,
                                                              configurable: true}),
                           TypeError);

    // Can't add new properties.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 3,
                                                       {value:4, enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);
    assertThrowsInstanceOf(() => Object.defineProperty(a, 10,
                                                       {value:4,
                                                        enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);

    assertEq(a.toString(), "4,7,3,,5");
}
for (var i = 0; i < 15; i++)
    testSealed();

function testFrozen() {
    var a = [1, 2, 3, , 5];
    Object.freeze(a);

    // Can redefine with same value/attributes.
    Object.defineProperty(a, 0, {value:1, enumerable: true, configurable: false, writable: false});

    // Can't change the value.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 1,
                                                       {value:7,
                                                        enumerable: true,
                                                        configurable: false,
                                                        writable: false}),
                           TypeError);

    // Can't make configurable, non-enumerable.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 2,
                                                       {value:3,
                                                        enumerable: true,
                                                        configurable: true,
                                                        writable: false}),
                           TypeError);
    assertThrowsInstanceOf(() => Object.defineProperty(a, 2,
                                                       {value:3,
                                                        enumerable: false,
                                                        configurable: false,
                                                        writable: false}),
                           TypeError);
    // Can't turn into an accessor.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 4, {get:() => -2,
                                                              enumerable: true,
                                                              configurable: true}),
                           TypeError);

    // Can't add new properties.
    assertThrowsInstanceOf(() => Object.defineProperty(a, 3,
                                                       {value:4, enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);
    assertThrowsInstanceOf(() => Object.defineProperty(a, 10,
                                                       {value:4,
                                                        enumerable: true,
                                                        configurable: true,
                                                        writable: true}),
                           TypeError);

    assertEq(a.toString(), "1,2,3,,5");
}
for (var i = 0; i < 15; i++)
    testFrozen();
