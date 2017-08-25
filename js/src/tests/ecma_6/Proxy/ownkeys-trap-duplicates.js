/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'ownkeys-trap-duplicates.js';
var BUGNUMBER = 1293995;
var summary =
  "Scripted proxies' [[OwnPropertyKeys]] should not throw if the trap " +
  "implementation returns duplicate properties and the object is " +
  "non-extensible or has non-configurable properties";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var target = Object.preventExtensions({ a: 1 });
var proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertDeepEq(Object.getOwnPropertyNames(proxy), ["a", "a"]);

target = Object.freeze({ a: 1 });
proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertDeepEq(Object.getOwnPropertyNames(proxy), ["a", "a"]);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
