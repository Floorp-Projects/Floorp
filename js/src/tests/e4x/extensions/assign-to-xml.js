// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

try
{
  eval("d, {" +
       "  x: [{" +
       "    x: x::x" +
       "  }]" +
       "} = q");
  throw new Error("didn't throw?");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true, "expected syntax error, got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
