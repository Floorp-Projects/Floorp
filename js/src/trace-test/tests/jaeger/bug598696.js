function f() {
  eval();
  var i = 0;
  assertEq(++i, 1);
}
f();
