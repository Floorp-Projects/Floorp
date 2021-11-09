function escape() { with ({}) {} }

function foo(i) {
  return i;
}

function bar(n, ...args) {
  escape(args);
  return foo.apply({}, args);
}

// |bar| must be small enough to be inlinable.
assertEq(isSmallFunction(bar), true);

function baz(a, n) {
  return bar(n, n);
}

var sum = 0;
for (var i = 0; i < 10000; i++) {
  sum += baz(0, 1);
}
assertEq(sum, 10000);
