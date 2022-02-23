// Parse-time static string lookup.
var s = "\ue531\ue531\ue531";
assertEq(s.codePointAt(0), 0xe531);
assertEq(s.codePointAt(1), 0xe531);
assertEq(s.codePointAt(2), 0xe531);

// Runtime static string lookup.
s = "\ue531\ue531\ue5310n".split(/\d[^]/)[0];
assertEq(s.codePointAt(0), 0xe531);
assertEq(s.codePointAt(1), 0xe531);
assertEq(s.codePointAt(2), 0xe531);
