function test() {
    // Latin1
    var s = toLatin1("  \r\t\n\u00A0foo 123\t \r\n\u00A0");

    var res = s.trim();
    assertEq(isLatin1(res), true);
    assertEq(res, "foo 123");

    res = s.trimLeft();
    assertEq(isLatin1(res), true);
    assertEq(res, "foo 123\t \r\n\u00A0");

    res = s.trimRight();
    assertEq(isLatin1(res), true);
    assertEq(res, "  \r\t\n\u00A0foo 123");

    res = toLatin1("foo 1234").trim();
    assertEq(isLatin1(res), true);
    assertEq(res, "foo 1234");

    // TwoByte
    s = "  \r\t\n\u00A0\u2000foo\u1200123\t \r\n\u00A0\u2009";
    assertEq(s.trim(), "foo\u1200123");
    assertEq(s.trimLeft(), "foo\u1200123\t \r\n\u00A0\u2009");
    assertEq(s.trimRight(), "  \r\t\n\u00A0\u2000foo\u1200123");
}
test();
