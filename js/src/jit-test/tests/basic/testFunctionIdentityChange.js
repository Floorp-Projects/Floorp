function testFunctionIdentityChange()
{
  function a() {}
  function b() {}

  var o = { a: a, b: b };

  for (var prop in o)
  {
    for (var i = 0; i < 1000; i++)
      o[prop]();
  }

  return true;
}
assertEq(testFunctionIdentityChange(), true);
