// Latin1
var re = new RegExp(toLatin1("foo[bB]a\\r"), toLatin1("im"));
assertEq(isLatin1(re.source), true);
assertEq(re.source, "foo[bB]a\\r");
assertEq(re.multiline, true);
assertEq(re.ignoreCase, true);
assertEq(re.sticky, false);

// TwoByte
re = new RegExp("foo[bB]a\\r\u1200", "im");
assertEq(isLatin1(re.source), false);
assertEq(re.source, "foo[bB]a\\r\u1200");
assertEq(re.multiline, true);
assertEq(re.ignoreCase, true);
assertEq(re.sticky, false);
