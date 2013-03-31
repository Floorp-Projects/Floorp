// |reftest| skip-if(!xulRuntime.shell) -- needs newGlobal()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = 'preventExtensions-cross-global.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 789897;
var summary =
  "Object.preventExtensions and Object.isExtensible should work correctly " +
  "across globals";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var otherGlobal = newGlobal();

var obj = {};
assertEq(otherGlobal.Object.isExtensible(obj), true);
assertEq(otherGlobal.Object.preventExtensions(obj), obj);
assertEq(otherGlobal.Object.isExtensible(obj), false);

var objFromOther = otherGlobal.Object();
assertEq(Object.isExtensible(objFromOther), true);
assertEq(Object.preventExtensions(objFromOther), objFromOther);
assertEq(Object.isExtensible(objFromOther), false);

var proxy = new Proxy({}, {});
assertEq(otherGlobal.Object.isExtensible(proxy), true);
assertEq(otherGlobal.Object.preventExtensions(proxy), proxy);
assertEq(otherGlobal.Object.isExtensible(proxy), false);

var proxyFromOther = otherGlobal.evaluate("new Proxy({}, {})");
assertEq(Object.isExtensible(proxyFromOther), true);
assertEq(Object.preventExtensions(proxyFromOther), proxyFromOther);
assertEq(Object.isExtensible(proxyFromOther), false);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
