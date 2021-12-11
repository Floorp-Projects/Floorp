var a = [1,2,3,4,5];

// Basic overflow in hoisted condition.
function foo(x, n) {
  var v = 0;
  for (var i = 0; i < n; i++)
    v += x[i];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo(a, 5), 15);
assertEq(foo(a, 6), NaN);

// Basic underflow in hoisted condition.
function foo2(x, m, n) {
  var v = 0;
  for (var i = m; i < n; i++)
    v += x[i];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo2(a, 0, 5), 15);
assertEq(foo2(a, -1, 5), NaN);

// Overflow when hoisting consolidated condition.
function foo3(x, m, n) {
  var v = 0;
  for (var i = m; i < n; i++)
    v += x[i] + x[i + 1] + x[i + 2];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo3(a, 0, 3), 27);
assertEq(foo3(a, 0, 4), NaN);

// Underflow when hoisting consolidated condition.
function foo4(x, m, n) {
  var v = 0;
  for (var i = m; i < n; i++)
    v += x[i] + x[i - 1] + x[i - 2];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo4(a, 2, 5), 27);
assertEq(foo4(a, 0, 5), NaN);

// Underflow due to decreasing index.
function foo5(x, n) {
  var v = 0;
  for (var i = 0; i < n; i++) {
    v += x[i];
    if (n == 4)
      i -= 2;
    if (i == -5)
      break;
  }
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo5(a, 5), 15);
assertEq(foo5(a, 4), NaN);

// Overflow after access at constant offset.
function foo6(x, m, n) {
  var v = 0;
  for (var i = m; i < n; i++)
    v += x[i + 10];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo6(a, -10, -5), 15);
assertEq(foo6(a, -10, -4), NaN);

// Underflow after access at constant offset.
function foo7(x, m, n) {
  var v = 0;
  for (var i = m; i < n; i++)
    v += x[i + 10];
  return v;
}
for (var i = 0; i < 15; i++)
  assertEq(foo7(a, -10, -5), 15);
assertEq(foo7(a, -11, -5), NaN);
