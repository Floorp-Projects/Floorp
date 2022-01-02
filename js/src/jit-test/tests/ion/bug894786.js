(function() {
  "use asm";
  function f(z)
  {
    z = z|0;
    return (((0xc0000000 >>> z) >> 0) % -1)|0;
  }
  return f;
})()(0);
