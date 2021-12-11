// Latin1
var s = "abcdef,g,,";
var res = s.split(",");
assertEq(res[0], "abcdef");
assertEq(isLatin1(res[0]), true);
assertEq(res[1], "g");
assertEq(res[2], "");
assertEq(res[3], "");

s = "abcdef,gh,,";
res = s.split("\u1200");
assertEq(res[0], "abcdef,gh,,");
assertEq(isLatin1(res[0]), true);

// TwoByte
s = "abcdef\u1200\u1200,g,,";
res = s.split(",");
assertEq(res[0], "abcdef\u1200\u1200");
assertEq(isLatin1(res[0]), false);
assertEq(res[1], "g");
assertEq(res[2], "");
assertEq(res[3], "");

res = s.split("\u1200");
assertEq(res[0], "abcdef");
assertEq(res[1], "");
assertEq(res[2], ",g,,");
