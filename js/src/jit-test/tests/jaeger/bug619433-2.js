
function foo(x) {
  var y = 2.5;
  y = -x;
  var z = [1,2,y];
  return x + 5;
}
for (var i = 0; i < 20; i++)
  foo(i);
assertEq(foo(20), 25);
