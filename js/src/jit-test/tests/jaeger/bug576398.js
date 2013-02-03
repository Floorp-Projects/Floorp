function K(x) {
  with ({});    // prevent compilation
  this.x = x; 
}
function f() {
  var a = new K(1);
  var b = new K(2);
  return (a == b);
}
assertEq(f(), false);
