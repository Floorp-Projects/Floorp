/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'ownkeys-trap-duplicates.js';
var BUGNUMBER = 1257779;
var summary =
  "Scripted proxies' [[OwnPropertyKeys]] should throw if the trap " +
  "implementation returns duplicate properties and the object is " +
  "non-extensible or has non-configurable properties";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var target = Object.preventExtensions({ a: 1 });

var proxy = new Proxy(target, { ownKeys(t) { return ['a', 'a']; } });

assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy),
                       TypeError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
