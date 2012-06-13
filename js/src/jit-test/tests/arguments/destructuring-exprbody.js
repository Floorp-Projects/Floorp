// See bug 763313
function f([a]) a
var i = 0;
var o = {get 0() { i++; return 42; }};
assertEq(f(o), 42);
assertEq(i, 1);
