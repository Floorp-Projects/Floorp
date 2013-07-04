function f(x)
{
  if (Math.imul(0xffffffff, x)) {
    return -x;
  }
  return 1;
}
f(0);
f(0);
