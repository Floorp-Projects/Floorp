/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 518663;
var summary = 'Object.getOwnPropertyNames: function objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function two(a, b) { }

assertEq(Object.getOwnPropertyNames(two).indexOf("length") >= 0, true);

var bound0 = Function.prototype.bind
           ? two.bind("this")
           : function two(a, b) { };

assertEq(Object.getOwnPropertyNames(bound0).indexOf("length") >= 0, true);
assertEq(bound0.length, 2);

var bound1 = Function.prototype.bind
           ? two.bind("this", 1)
           : function one(a) { };

assertEq(Object.getOwnPropertyNames(bound1).indexOf("length") >= 0, true);
assertEq(bound1.length, 1);

var bound2 = Function.prototype.bind
           ? two.bind("this", 1, 2)
           : function zero() { };

assertEq(Object.getOwnPropertyNames(bound2).indexOf("length") >= 0, true);
assertEq(bound2.length, 0);

var bound3 = Function.prototype.bind
           ? two.bind("this", 1, 2, 3)
           : function zero() { };

assertEq(Object.getOwnPropertyNames(bound3).indexOf("length") >= 0, true);
assertEq(bound3.length, 0);


/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
