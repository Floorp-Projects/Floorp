
function foo() {
  var x = {};
  x.__proto__ = function() { return 0 }
  return x;
}
var a = foo();
var b = foo();
assertEq(a.__proto__ === b.__proto__, false);
