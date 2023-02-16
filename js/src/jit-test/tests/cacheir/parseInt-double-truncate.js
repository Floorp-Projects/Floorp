// Test some double-truncation edge cases with parseInt(double).

function testPos1() {
    with({}) {}
    var fun = d => parseInt(d);
    for (var i = 0; i < 2000; i++) {
        assertEq(fun(i + 0.5), i);
    }
    assertEq(fun(0xf_ffff_ffff) + 345, 68719477080);
}
testPos1();

function testPos2() {
    with({}) {}
    var fun = d => parseInt(d);
    for (var i = 0; i < 2000; i++) {
        assertEq(fun(i + 0.5), i);
    }
    assertEq(fun(0x8000_0000) + 345, 2147483993);
}
testPos2();

function testNeg1() {
    with({}) {}
    var fun = d => parseInt(d);
    for (var i = 0; i < 2000; i++) {
        assertEq(fun(i + 0.5), i);
    }
    assertEq(fun(-0xf_ffff_ffff) - 345, -68719477080);
}
testNeg1();

function testNeg2() {
    with({}) {}
    var fun = d => parseInt(d);
    for (var i = 0; i < 2000; i++) {
        assertEq(fun(i + 0.5), i);
    }
    assertEq(fun(-0x8000_0001) - 345, -2147483994);
}
testNeg2();
