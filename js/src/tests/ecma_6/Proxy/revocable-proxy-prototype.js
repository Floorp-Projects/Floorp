/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'revocable-proxy-prototype.js';
var BUGNUMBER = 1052139;
var summary = "Accessing a revocable proxy's [[Prototype]] shouldn't crash";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/
function checkFunctionAppliedToRevokedProxy(fun)
{
  var p = Proxy.revocable({}, {});
  p.revoke();

  try
  {
    fun(p.proxy);
    throw "didn't throw";
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "expected TypeError, got " + e);
  }
}

checkFunctionAppliedToRevokedProxy(proxy => Object.getPrototypeOf(proxy));
checkFunctionAppliedToRevokedProxy(proxy => Object.setPrototypeOf(proxy, null));

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
