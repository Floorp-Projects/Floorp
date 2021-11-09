function foo(...args) {
  args[0] = 3;
  return bar.apply({}, args);
}

// |foo| must be small enough to be inlinable.
assertEq(isSmallFunction(foo), true);

function bar(x, y) {
  return x + y;
}

with ({}) {}

var sum = 0;
for (var i = 0; i < 100; i++) {
  sum += foo(1, 2);
}
assertEq(sum, 500);
