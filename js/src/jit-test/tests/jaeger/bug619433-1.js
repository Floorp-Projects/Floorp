
/* Test compiling JSOP_STRICTEQ on known doubles. */

function foo(x) {
  return x === x;
}

for (var i = 0; i < 20; i++) {
  assertEq(foo(1.2), true);
  assertEq(foo(NaN), false);
}

function bar(x) {
  if (x === x)
    return true;
  return false;
}

for (var i = 0; i < 20; i++) {
  assertEq(bar(1.2), true);
  assertEq(bar(NaN), false);
}
