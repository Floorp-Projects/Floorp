function negZeroMinusNegZero()
{
  var x = -0.0;
  var y = -0.0;
  return +(x - y);
}

assertEq(1 / negZeroMinusNegZero(), Infinity);
assertEq(1 / negZeroMinusNegZero(), Infinity);
