function f(z)
{
  return (((0x80000000 | 0) % (0x80000001 | z)) | 0) % 100000
}
assertEq(f(0), -1);
assertEq(f(0), -1);
