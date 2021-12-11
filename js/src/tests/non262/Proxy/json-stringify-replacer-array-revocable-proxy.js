// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = "json-stringify-replacer-array-revocable-proxy.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 1196497;
var summary =
  "Don't assert when JSON.stringify is passed a revocable proxy to an array, " +
  "then that proxy is revoked midflight during stringification";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [];
var { proxy, revoke } = Proxy.revocable(arr, {
  get(thisv, prop, receiver) {
    // First (and only) get will be for "length", to determine the length of the
    // list of properties to serialize.  Returning 0 uses the empty list,
    // resulting in |a: 0| being ignored below.
    assertEq(thisv, arr);
    assertEq(prop, "length");
    assertEq(receiver, proxy);

    revoke();
    return 0;
  }
});

assertEq(JSON.stringify({a: 0}, proxy), "{}");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
