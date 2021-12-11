
/* Test concat compiler paths. */

for (var i = 9; i < 10; i++)
  assertEq([2].concat([3])[0], 2);

function f(a, b) {
  return a.concat(b)[0];
}
function g() {
  var x = [];
  var y = [1];
  for (var i = 0; i < 50; i++)
    assertEq(f(x, y), 1);
  eval('y[0] = "three"');
  assertEq(f(x, y), "three");
}
g();
