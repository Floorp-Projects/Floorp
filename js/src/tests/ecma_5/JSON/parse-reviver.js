function doubler(k, v)
{
  assertEq(typeof k, "string");

  if (typeof v == "number")
    return 2 * v;

  return v;
}

var x = JSON.parse('{"a":5,"b":6}', doubler);
assertEq(x.hasOwnProperty('a'), true);
assertEq(x.hasOwnProperty('b'), true);
assertEq(x.a, 10);
assertEq(x.b, 12);

x = JSON.parse('[3, 4, 5]', doubler);
assertEq(x[0], 6);
assertEq(x[1], 8);
assertEq(x[2], 10);

// make sure reviver isn't called after a failed parse
var called = false;
function dontCallMe(k, v)
{
  called = true;
}

try
{
  JSON.parse('{{{{{{{}}}}', dontCallMe);
  throw new Error("didn't throw?");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true, "wrong exception: " + e);
}
assertEq(called, false);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
