function testAddNull()
{
  var rv;
  for (var x = 0; x < 9; ++x)
    rv = null + [,,];
  return rv;
}
assertEq(testAddNull(), "null,");
