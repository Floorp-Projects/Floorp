var s = toLatin1('Abc1234"\'\t987\u00ff');
assertEq(s.toSource(), '(new String("Abc1234\\"\'\\t987\\xFF"))');
assertEq(s.quote(), '"Abc1234\\"\'\\t987\\xFF"');

s = 'Abc1234"\'\t\u1200987\u00ff';
assertEq(s.toSource(), '(new String("Abc1234\\"\'\\t\\u1200987\\xFF"))');
assertEq(s.quote(), '"Abc1234\\"\'\\t\\u1200987\\xFF"');
