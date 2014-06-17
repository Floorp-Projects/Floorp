var arrLatin1 = [toLatin1("abc1"), toLatin1("abc\u00A0")];
assertEq(arrLatin1.join(toLatin1("sep\u00ff")), "abc1sep\xFFabc\xA0");

var arrTwoByte = [toLatin1("abc2"), "def\u1200"];
assertEq(arrTwoByte.join(toLatin1("sep\u00fe")), "abc2sep\xFEdef\u1200");

assertEq(arrLatin1.join(toLatin1("-")), "abc1-abc\xA0");
assertEq(arrTwoByte.join(toLatin1("7")), "abc27def\u1200");

assertEq(arrLatin1.join("\u1200"), "abc1\u1200abc\xA0");
assertEq(arrTwoByte.join("\u1200"), "abc2\u1200def\u1200");

assertEq(arrLatin1.join("---\u1200"), "abc1---\u1200abc\xA0");
assertEq(arrTwoByte.join("---\u1200"), "abc2---\u1200def\u1200");
