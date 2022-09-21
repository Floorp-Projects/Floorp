with ({}); // Don't inline anything into the top-level script.

function f(x) {
  return Math.min(Math.max(x / x, x), x);
}

for (var i = 0; i < 100; ++i) {
  f(1);
}

assertEq(f(0), NaN);
