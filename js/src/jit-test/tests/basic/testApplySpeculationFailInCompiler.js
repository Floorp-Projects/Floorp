function baz(i) { return i }
function bar(x, y) { return baz.apply(null, x ? y : arguments) }
var s;
for (var i = 0; i < 1000; ++i)
  s = bar(true, [42]);
assertEq(s, 42);
