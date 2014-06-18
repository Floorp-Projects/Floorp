// Latin1
s = toLatin1("a%2b%20def%00A0");
assertEq(unescape(s), "a+ def\x00A0");

s = toLatin1("a%2b%20def%00A0%u1200");
assertEq(unescape(s), "a+ def\x00A0\u1200");

// TwoByte
s += "\u1200";
assertEq(unescape(s), "a+ def\x00A0\u1200\u1200");

// Latin1
s = toLatin1("abc \u00ff");
assertEq(escape(s), "abc%20%FF");

// TwoByte
s += "\u1200";
assertEq(escape(s), "abc%20%FF%u1200");
