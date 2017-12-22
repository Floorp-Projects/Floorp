/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Author: Emilio Cobos Álvarez <ecoal95@gmail.com>
 */
var BUGNUMBER = 1312948;
var summary = "Freezing a dictionary mode object with a length property should make Object.isFrozen report true";

print(BUGNUMBER + ": " + summary);

/* Convert to dictionary mode */
delete Array.prototype.slice;

Object.freeze(Array.prototype);
assertEq(Object.isFrozen(Array.prototype), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
