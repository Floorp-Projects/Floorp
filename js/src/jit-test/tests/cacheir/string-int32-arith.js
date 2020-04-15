function test(zero, one) {
    assertEq(10 - zero, 10);
    assertEq(10 - one, 9);
    assertEq(zero - 0, 0);
    assertEq(one - 1, 0);

    assertEq(10 * zero, 0);
    assertEq(zero * 10, 0);
    assertEq(10 * one, 10);
    assertEq(one * 10, 10);

    assertEq(10 / one, 10);
    assertEq(one / 1, 1);
    assertEq(10 % one, 0);
    assertEq(one % 1, 0);

    assertEq(10 ** one, 10);
    assertEq(one ** 4, 1);
}

for (var i = 0; i < 10; i++) {
    test(0, 1);
    test('0', '1');
    test('0x0', '0x1');
    test('0.0', '1.0');
}
