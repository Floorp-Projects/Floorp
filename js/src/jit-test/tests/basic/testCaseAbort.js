function testCaseAbort()
{
  var four = "4";
  var r = 0;
  for (var i = 0; i < 5; i++)
  {
    switch (i)
    {
      case four: r += 1; break;
      default: r += 2; break;
    }
  }

  return "" + r;
}
assertEq(testCaseAbort(), "10");
