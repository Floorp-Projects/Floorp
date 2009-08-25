function testInvertNullAfterNegateNull()
{
  for (var i = 0; i < 5; i++) !null;
  for (var i = 0; i < 5; i++) -null;
  return "no assertion";
}
assertEq(testInvertNullAfterNegateNull(), "no assertion");
