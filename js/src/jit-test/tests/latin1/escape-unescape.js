// Latin1
s = toLatin1("a%2b%20def%00A0");
assertEq(unescape(s), "a+ def\x00A0");

s = toLatin1("a%2b%20def%00A0%u1200");
assertEq(unescape(s), "a+ def\x00A0\u1200");

// TwoByte
s += "\u1200";
assertEq(unescape(s), "a+ def\x00A0\u1200\u1200");
