function g()
{
  "use asm";
  function f()
  {
    return (0 > (0x80000000 | 0)) | 0;
  }
  return f;
}

assertEq(g()(), (0 > (0x80000000 | 0)) | 0);
