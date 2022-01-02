function toLatin1(s) {
    assertEq(isLatin1(s), true);
    return s;
}

var arrLatin1 = [toLatin1("abc1"), toLatin1("abc\u00A0")];
var res = arrLatin1.join(toLatin1("sep\u00ff"));
assertEq(res, "abc1sep\xFFabc\xA0");
assertEq(isLatin1(res), true);

var arrTwoByte = [toLatin1("abc2"), "def\u1200"];
assertEq(arrTwoByte.join(toLatin1("sep\u00fe")), "abc2sep\xFEdef\u1200");

res = arrLatin1.join(toLatin1("-"));
assertEq(res, "abc1-abc\xA0");
assertEq(isLatin1(res), true);

assertEq(arrTwoByte.join(toLatin1("7")), "abc27def\u1200");

assertEq(arrLatin1.join("\u1200"), "abc1\u1200abc\xA0");
assertEq(arrTwoByte.join("\u1200"), "abc2\u1200def\u1200");

assertEq(arrLatin1.join("---\u1200"), "abc1---\u1200abc\xA0");
assertEq(arrTwoByte.join("---\u1200"), "abc2---\u1200def\u1200");
