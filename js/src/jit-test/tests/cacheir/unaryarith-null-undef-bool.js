function test(input, resPos, resNeg, resToNumeric, resInc, resDec) {
    assertEq(+input, resPos);
    assertEq(-input, resNeg);

    var input1 = input;
    assertEq(input1++, resToNumeric);
    assertEq(input1, resInc);

    var input2 = input;
    assertEq(++input2, resInc);
    assertEq(input1, resInc);

    var input3 = input;
    assertEq(input3--, resToNumeric);
    assertEq(input3, resDec);

    var input4 = input;
    assertEq(--input4, resDec);
    assertEq(input4, resDec);
}
for (var i = 0; i < 50; i++) {
    test(null, 0, -0, 0, 1, -1);
    test(undefined, NaN, NaN, NaN, NaN, NaN);
    test(true, 1, -1, 1, 2, 0);
    test(false, 0, -0, 0, 1, -1);
}
