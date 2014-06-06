function testParseInt() {
    // Latin1
    assertEq(parseInt(toLatin1("12345abc")), 12345);
    assertEq(parseInt(toLatin1("0x5")), 0x5);
    assertEq(parseInt(toLatin1("-123")), -123);
    assertEq(parseInt(toLatin1("xyz")), NaN);
    assertEq(parseInt(toLatin1("1234GHI"), 17), 94298);
    assertEq(parseInt(toLatin1("9007199254749999")), 9007199254750000);
    assertEq(parseInt(toLatin1("  9007199254749998"), 16), 10378291982571444000);

    // TwoByte
    assertEq(parseInt("12345abc\u1200"), 12345);
    assertEq(parseInt("0x5\u1200"), 0x5);
    assertEq(parseInt("  -123\u1200"), -123);
    assertEq(parseInt("\u1200"), NaN);
    assertEq(parseInt("1234GHI\u1200", 17), 94298);
    assertEq(parseInt("9007199254749999\u1200"), 9007199254750000);
    assertEq(parseInt("  9007199254749998\u1200", 16), 10378291982571444000);
}
testParseInt();

function testParseFloat() {
    // Latin1
    assertEq(parseFloat(toLatin1("3.1415")), 3.1415);
    assertEq(parseFloat(toLatin1("  -1234")), -1234);
    assertEq(parseFloat(toLatin1("\u00AA")), NaN);
    assertEq(parseFloat(toLatin1("Infinityabc")), Infinity);
    assertEq(parseFloat(toLatin1("-Infinity")), -Infinity);
    assertEq(parseFloat(toLatin1("\t\t\t+Infinity")), Infinity);

    // TwoByte
    assertEq(parseFloat("3.1415\u0FFF"), 3.1415);
    assertEq(parseFloat("  -1234\u0FFF"), -1234);
    assertEq(parseFloat("\u00AA\u0FFF"), NaN);
    assertEq(parseFloat("Infinityabc\u0FFF"), Infinity);
    assertEq(parseFloat("-Infinity\u0FFF"), -Infinity);
    assertEq(parseFloat("\t\t\t+Infinity\u0FFF"), Infinity);
}
testParseFloat();
