
// test local/arg liveness analysis in presence of many locals

function foo(a, b, c) {
  var x = 0, y = 0, z = 0;
  if (a < b) {
    x = a + 0;
    y = b + 0;
    z = c + 0;
  } else {
    x = a;
    y = b;
    z = c;
  }
  return x + y + z;
}
assertEq(foo(1, 2, 3), 6);
