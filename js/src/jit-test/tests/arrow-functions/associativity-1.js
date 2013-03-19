// Arrow right-associativity.

var t = a => b => a;
assertEq(t('A')('B'), 'A');

var curry = f => a => b => f(a, b);
var curried_atan2 = curry(Math.atan2);
assertEq(curried_atan2(0)(1), 0);
