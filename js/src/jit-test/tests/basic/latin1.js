var s = "Foo123";
assertEq(isLatin1(s), false);

s = toLatin1(s);
assertEq(isLatin1(s), true);
