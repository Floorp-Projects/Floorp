/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1282047;
var summary = 'Infinite recursion via Array.isArray on a proxy';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var proxy = Proxy.revocable([], {}).proxy;

// A depth of 100000 ought to be enough for any platform to consume its entire
// stack, hopefully without making any recalcitrant platforms time out.  If no
// timeout happens, the assertEq checks for the proper expected value.
for (var i = 0; i < 1e5; i++)
  proxy = new Proxy(proxy, {});

try
{
  assertEq(Array.isArray(proxy), true);

  // If we reach here, it's cool, we just didn't consume the entire stack.
}
catch (e)
{
  assertEq(e instanceof InternalError, true,
           "should have thrown for over-recursion");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
