var s = 'Abc1234"\'\t987\u00ff';
assertEq(isLatin1(s), true);
assertEq(s.toSource(), '(new String("Abc1234\\"\'\\t987\\xFF"))');
var res = s.quote();
assertEq(res, '"Abc1234\\"\'\\t987\\xFF"');
assertEq(isLatin1(res), true);

s = 'Abc1234"\'\t\u1200987\u00ff';
assertEq(isLatin1(s), false);
assertEq(s.toSource(), '(new String("Abc1234\\"\'\\t\\u1200987\\xFF"))');
assertEq(s.quote(), '"Abc1234\\"\'\\t\\u1200987\\xFF"');
