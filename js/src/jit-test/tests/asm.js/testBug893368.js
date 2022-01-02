function m(stdlib)
{
  "use asm";
  var abs = stdlib.Math.abs;
  function f(p)
  {
    p = p|0;
    return ((abs(p|0)|0) % 3)|0;
  }
  return f;
}
var f = m(this);
assertEq(f(0x80000000), -2);
