function testBitOrAnyInconvertibleObject()
{
  var count = 0;
  function toString() { ++count; if (count == 95) return {}; return count; }
  var o = {valueOf: undefined, toString: toString};

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
        var q = 1 | o;
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== 95)
      return "expected q === 95, got " + q;
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with 2 | o"; // hey, a rhyme!

  return "pass";
}
assertEq(testBitOrAnyInconvertibleObject(), "pass");
