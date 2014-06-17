// Latin1
var s = toLatin1("abcdef,g,,");
var res = s.split(toLatin1(","));
assertEq(res[0], "abcdef");
assertEq(res[1], "g");
assertEq(res[2], "");
assertEq(res[3], "");

s = toLatin1("abcdef,gh,,");
res = s.split("\u1200");
assertEq(res[0], "abcdef,gh,,");

// TwoByte
s = "abcdef\u1200\u1200,g,,";
res = s.split(",");
assertEq(res[0], "abcdef\u1200\u1200");
assertEq(res[1], "g");
assertEq(res[2], "");
assertEq(res[3], "");

res = s.split("\u1200");
assertEq(res[0], "abcdef");
assertEq(res[1], "");
assertEq(res[2], ",g,,");
