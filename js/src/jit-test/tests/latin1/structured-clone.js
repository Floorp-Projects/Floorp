// Latin1
var s = deserialize(serialize("foo123\u00EE"));
assertEq(s, "foo123\u00EE");
assertEq(isLatin1(s), true);

var o = deserialize(serialize(new String("foo\u00EE")));
assertEq(typeof o, "object");
assertEq(o.valueOf(), "foo\u00EE");
assertEq(isLatin1(o.valueOf()), true);

// TwoByte
var s = deserialize(serialize("foo123\u00FF\u1234"));
assertEq(s, "foo123\u00FF\u1234");
assertEq(isLatin1(s), false);

var o = deserialize(serialize(new String("foo\uEEEE")));
assertEq(typeof o, "object");
assertEq(o.valueOf(), "foo\uEEEE");
assertEq(isLatin1(o.valueOf()), false);
