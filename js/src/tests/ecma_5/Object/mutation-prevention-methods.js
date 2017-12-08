/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = 'mutation-prevention-methods.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 492849;
var summary = 'Object.is{Sealed,Frozen}, Object.{seal,freeze}';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Empty object

var o1 = {};

assertEq(Object.isExtensible(o1), true);
assertEq(Object.isSealed(o1), false);
assertEq(Object.isFrozen(o1), false);

Object.preventExtensions(o1);

// An non-extensible empty object has no properties, so it is vacuously sealed
// and frozen.
assertEq(Object.isExtensible(o1), false);
assertEq(Object.isSealed(o1), true);
assertEq(Object.isFrozen(o1), true);


// Object with a data property

var o2 = { 1: 2 };

assertEq(Object.isExtensible(o2), true);
assertEq(Object.isSealed(o2), false);
assertEq(Object.isFrozen(o2), false);

Object.preventExtensions(o2);

assertEq(Object.isExtensible(o2), false);
assertEq(Object.isSealed(o2), false);
assertEq(Object.isFrozen(o2), false);

Object.seal(o2);

assertEq(Object.isExtensible(o2), false);
assertEq(Object.isSealed(o2), true);
assertEq(Object.isFrozen(o2), false);

assertEq(o2[1], 2);

var desc;

desc = Object.getOwnPropertyDescriptor(o2, "1");
assertEq(typeof desc, "object");
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);
assertEq(desc.value, 2);
assertEq(desc.writable, true);

o2[1] = 17;

assertEq(o2[1], 17);

desc = Object.getOwnPropertyDescriptor(o2, "1");
assertEq(typeof desc, "object");
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);
assertEq(desc.value, 17);
assertEq(desc.writable, true);

Object.freeze(o2);

assertEq(o2[1], 17);

desc = Object.getOwnPropertyDescriptor(o2, "1");
assertEq(typeof desc, "object");
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);
assertEq(desc.value, 17);
assertEq(desc.writable, false);


// Object with an accessor property

var o3 = { get foo() { return 17; } };

assertEq(Object.isExtensible(o3), true);
assertEq(Object.isSealed(o3), false);
assertEq(Object.isFrozen(o3), false);

Object.preventExtensions(o3);

assertEq(Object.isExtensible(o3), false);
assertEq(Object.isSealed(o3), false);
assertEq(Object.isFrozen(o3), false);

Object.seal(o3);

// An accessor property in a sealed object is unchanged if that object is
// frozen, so a sealed object containing only accessor properties is also
// vacuously frozen.
assertEq(Object.isExtensible(o3), false);
assertEq(Object.isSealed(o3), true);
assertEq(Object.isFrozen(o3), true);

Object.freeze(o3);

assertEq(Object.isExtensible(o3), false);
assertEq(Object.isSealed(o3), true);
assertEq(Object.isFrozen(o3), true);


/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
