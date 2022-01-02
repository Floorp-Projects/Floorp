function testDoubleComparison()
{
  for (var i = 0; i < 500000; ++i)
  {
    switch (1 / 0)
    {
      case Infinity:
    }
  }

  return "finished";
}
assertEq(testDoubleComparison(), "finished");
