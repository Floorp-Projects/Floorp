function f(a) {
    assertEq(arguments[0], 1);

    Object.defineProperty(arguments, 0, {value: 23, writable: true, configurable: true});
    assertEq(arguments[0], 23);
    assertEq(a, 23);

    a = 12;
    assertEq(a, 12);
    assertEq(arguments[0], 12);

    Object.defineProperty(arguments, 0, {value: 9, writable: false, configurable: false});
    assertEq(arguments[0], 9);
    assertEq(a, 9);

    a = 4;
    assertEq(arguments[0], 9);
    assertEq(a, 4);
}
f(1);
