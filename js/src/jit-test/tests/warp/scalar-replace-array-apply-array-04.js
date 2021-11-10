var result;

function g(a, b) {
  with ({}) {}
  result = a + b;
}

function escape() { with({}) {} }

function f(...args) {
  escape(args);
  for (var i = 0; i < 50; ++i) {
    g.apply(this, args);
  }
}

// |f| must be small enough to be inlinable.
assertEq(isSmallFunction(f), true);

f(1, 2);
assertEq(result, 3);

f("");
assertEq(result, "undefined");
