function toLatin1(s) {
    assertEq(isLatin1(s), true);
    return s;
}
function testStartsWith() {
    var s1 = toLatin1("abc\u0099def");
    var s2 = toLatin1("abc\u0099d");
    var s3 = toLatin1("abc\u0098d");
    var s4 = toLatin1("bc\u0099");

    // Latin1 + Latin1
    assertEq(s1.startsWith(s2), true);
    assertEq(s1.startsWith(s3), false);
    assertEq(s1.startsWith(s4), false);
    assertEq(s1.startsWith(s4, 1), true);
    assertEq(s1.startsWith(s1), true);

    // Latin1 + TwoByte
    assertEq(s1.startsWith("abc\u0099\u1200".slice(0, -1)), true);
    assertEq(s1.startsWith("abc\u0099e\u1200".slice(0, -1)), false);
    assertEq(s1.startsWith("bc\u0099\u1200".slice(0, -1), 1), true);
    assertEq(s1.startsWith("\u1200"), false);

    // TwoByte + Latin1
    var s5 = "abc\u0099de\u1200";
    assertEq(s5.startsWith(s1), false);
    assertEq(s5.startsWith(s2), true);
    assertEq(s5.startsWith(s4), false);
    assertEq(s5.startsWith(s4, 1), true);

    // TwoByte + TwoByte
    assertEq(s5.startsWith(s5), true);
    assertEq(s5.startsWith("\u1200"), false);
    assertEq(s5.startsWith("\u1200", 6), true);
}
testStartsWith();

function testEndsWith() {
    var s1 = toLatin1("zabc\u0099defg");
    var s2 = toLatin1("\u0099defg");
    var s3 = toLatin1("\u0098defg");
    var s4 = toLatin1("zabc\u0099def");

    // Latin1 + Latin1
    assertEq(s1.endsWith(s2), true);
    assertEq(s1.endsWith(s3), false);
    assertEq(s1.endsWith(s4), false);
    assertEq(s1.endsWith(s4, 8), true);
    assertEq(s1.endsWith(s1), true);

    // Latin1 + TwoByte
    assertEq(s1.endsWith("abc\u0099defg\u1200".slice(0, -1)), true);
    assertEq(s1.endsWith("\u1100efg\u1200".slice(0, -1)), false);
    assertEq(s1.endsWith("bc\u0099\u1200".slice(0, -1), 5), true);
    assertEq(s1.endsWith("\u1200"), false);

    // TwoByte + Latin1
    var s5 = "\u1200zabc\u0099defg";
    assertEq(s5.endsWith(s1), true);
    assertEq(s5.endsWith(s2), true);
    assertEq(s5.endsWith(s4), false);
    assertEq(s5.endsWith(s4, 9), true);

    // TwoByte + TwoByte
    assertEq(s5.endsWith(s5), true);
    assertEq(s5.endsWith("\u1200"), false);
    assertEq(s5.endsWith("\u1200za", 3), true);
}
testEndsWith();
