// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function foo()
{
  assertEq(foo.arguments.length, 0);
  assertEq(foo.caller, null);
}

assertEq(foo.arguments, null);
assertEq(foo.caller, null);
foo();
assertEq(foo.arguments, null);
assertEq(foo.caller, null);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
