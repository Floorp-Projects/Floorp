function K(x) {
  var dontCompile = <a></a>;
  this.x = x; 
}
function f() {
  var a = new K(1);
  var b = new K(2);
  return (a == b);
}
assertEq(f(), false);
