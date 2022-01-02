var intMin = -2147483648;
var intMax = 2147483647;
var negativeZero = -0;
var zero = 0;

function testModNegativeZero() {
    assertEq(intMin % -2147483648, -0);
    assertEq(intMin % -1, -0);
    assertEq(intMin % 1, -0);
    assertEq(intMin % -2147483648, -0);

    assertEq(((intMin|0) % -2147483648)|0, 0);
    assertEq(((intMin|0) % -1)|0, 0);
    assertEq(((intMin|0) % 1)|0, 0);
    assertEq(((intMin|0) % -2147483648)|0, 0);
}

testModNegativeZero();
testModNegativeZero();

function testMinModulus1() {
    assertEq(intMin % -3, -2);
    assertEq(intMin % 3, -2);
    assertEq(intMin % 10, -8);
    assertEq(intMin % 2147483647, -1);

    assertEq(((intMin|0) % -3)|0, -2);
    assertEq(((intMin|0) % 3)|0, -2);
    assertEq(((intMin|0) % 10)|0, -8);
    assertEq(((intMin|0) % 2147483647)|0, -1);
}

testMinModulus1();
testMinModulus1();

function testMinModulus2() {
    for (var i = -10; i <= 10; ++i)
        assertEq(i % -2147483648, i);
    assertEq(intMax % -2147483648, intMax);

    for (var i = -10; i <= 10; ++i)
        assertEq(((i|0) % -2147483648)|0, i);
    assertEq(((intMax|0) % -2147483648)|0, intMax);
}

testMinModulus2();
testMinModulus2();

function testDivEdgeCases() {
    assertEq(intMin / -2147483648, 1);
    assertEq(intMin / -1, -intMin);
    assertEq(negativeZero / -2147483648, 0);
    assertEq(zero / -2147483648, -0);

    assertEq(((intMin|0) / -2147483648)|0, 1);
    assertEq(((intMin|0) / -1)|0, intMin);
    assertEq(((negativeZero|0) / -2147483648)|0, 0);
    assertEq(((zero|0) / -2147483648)|0, 0);
}

testDivEdgeCases();
testDivEdgeCases();
