// See bug 763313
function f([a]) a
var i = 0;
var o = {'@@iterator': function () { i++; return {
  next: function () { i++; return {value: 42, done: false}; }}}};
assertEq(f(o), 42);
assertEq(i, 2);
