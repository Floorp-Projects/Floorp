// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var r1 = [0, 1, 2, 3];
Object.defineProperty(r1, (1 << 23) - 1, {});
JSON.stringify({ 0: 0, 1: 1, 2: 2, 3: 3 }, r1)

var r2 = [0, 1, 2, 3];
Object.defineProperty(r2, (1 << 30), {});
try
{
  JSON.stringify({ 0: 0, 1: 1, 2: 2, 3: 3 }, r2)
}
catch (e)
{
  assertEq(""+e, "InternalError: allocation size overflow");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
