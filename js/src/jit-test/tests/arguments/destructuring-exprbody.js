// See bug 763313
load(libdir + "iteration.js");
function f([a]) a
var i = 0;
var o = {[Symbol.iterator]: function () { i++; return {
  next: function () { i++; return {value: 42, done: false}; }}}};
assertEq(f(o), 42);
assertEq(i, 2);
