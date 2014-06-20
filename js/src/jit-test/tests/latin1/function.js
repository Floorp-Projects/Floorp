function test() {
    var arg1TwoByte = "arg1\u1200";
    var arg2Latin1 = toLatin1("arg2");

    var bodyLatin1 = toLatin1("return arg2 * 3");

    var f = Function(arg1TwoByte, arg2Latin1, bodyLatin1);
    assertEq(f(10, 20), 60);
    assertEq(f.toSource().contains("arg1\u1200, arg2"), true);

    var bodyTwoByte = "return arg1\u1200 + arg2;";
    f = Function(arg1TwoByte, arg2Latin1, bodyTwoByte);
    assertEq(f(30, 40), 70);
    assertEq(f.toSource().contains("arg1\u1200, arg2"), true);
}
test();
