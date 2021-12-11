function testSwitchUndefined()
{
  var x = undefined;
  var y = 0;
  for (var i = 0; i < 5; i++)
  {
    switch (x)
    {
      default:
        y++;
    }
  }
  return y;
}
assertEq(testSwitchUndefined(), 5);
