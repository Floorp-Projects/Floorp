function testLastIndexOf() {
    var s1 = toLatin1("abcdefgh123456\u0081defg");
    var s2 = toLatin1("456\u0081de");

    // Latin1 + Latin1
    assertEq(s1.lastIndexOf(s1), 0);
    assertEq(s1.lastIndexOf(s2), 11);
    assertEq(s1.lastIndexOf(s2, 10), -1);
    assertEq(s2.lastIndexOf(s1), -1);

    // Latin1 + TwoByte
    assertEq(s1.lastIndexOf("abc\u1234"), -1);
    assertEq(s1.lastIndexOf("def\u1234".substring(0, 3)), 15);
    assertEq(s1.lastIndexOf("def\u1234".substring(0, 3), 9), 3);

    // TwoByte + Latin1
    var s3 = "123456\u0081defg\u1123a456\u0081defg";
    assertEq(isLatin1(s2), true);
    assertEq(s3.lastIndexOf(s2), 13);
    assertEq(s3.lastIndexOf(s2, 12), 3);
    assertEq(s3.lastIndexOf(toLatin1("defg7")), -1);

    // TwoByte + TwoByte
    assertEq(s3.lastIndexOf("\u1123a4"), 11);
    assertEq(s3.lastIndexOf("\u1123a4", 10), -1);
    assertEq(s3.lastIndexOf("\u1123a\u1098"), -1);
    assertEq(s3.lastIndexOf(s3), 0);
}
testLastIndexOf();
