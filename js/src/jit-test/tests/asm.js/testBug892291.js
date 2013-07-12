function a(stdlib) {
  "use asm";
  var imul = stdlib.Math.imul;
  function f() {
    return ((imul(-800, 0xf8ba1243)|0) % -1)|0;
  }
  return f;
}
var f = a(this);
assertEq(f(), 0);
