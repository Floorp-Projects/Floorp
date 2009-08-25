function arityMismatchMissingArg(arg)
{
  for (var a = 0, i = 1; i < 10000; i *= 2) {
    a += i;
  }
  return a;
}

function arityMismatchExtraArg()
{
  return arityMismatchMissingArg(1, 2);
}
assertEq(arityMismatchExtraArg(), 16383);
