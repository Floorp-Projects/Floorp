function testAddInconvertibleObjectAny()
{
  var count = 0;
  function toString()
  {
    ++count;
    if (count == 95)
      return {};
    return "" + count;
  }
  var o = {valueOf: undefined, toString: toString};

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
        var q = o + 5;
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== "945")
      return "expected q === '945', got " + q + " (type " + typeof q + ")";
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with o + 5";

  return "pass";
}
assertEq(testAddInconvertibleObjectAny(), "pass");
checkStats({
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 3
});
