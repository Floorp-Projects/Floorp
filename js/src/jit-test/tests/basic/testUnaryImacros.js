function testUnaryImacros()
{
  function checkArg(x)
  {
    return 1;
  }

  var o = { valueOf: checkArg, toString: null };
  var count = 0;
  var v = 0;
  for (var i = 0; i < 5; i++)
    v += +o + -(-o);

  var results = [v === 10 ? "valueOf passed" : "valueOf failed"];

  o.valueOf = null;
  o.toString = checkArg;

  for (var i = 0; i < 5; i++)
    v += +o + -(-o);

  results.push(v === 20 ? "toString passed" : "toString failed");

  return results.join(", ");
}
assertEq(testUnaryImacros(), "valueOf passed, toString passed");
