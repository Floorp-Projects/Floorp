function foo(x) {
  return bar(x);
}
function bar(x) {
  return x.f + 10;
}
var g = Object();
g.f = 10;
assertEq(foo(g), 20);
assertEq(foo(g), 20);
assertEq(foo(g), 20);
eval("g.f = 'three'");
assertEq(foo(g), 'three10');
