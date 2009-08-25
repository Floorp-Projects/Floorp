function xprop()
{
  a = 0;
  for (var i = 0; i < 20; i++)
    a += 7;
  return a;
}
assertEq(xprop(), 140);
