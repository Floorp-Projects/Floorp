/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 547087;
var summary = 'JS_EnumerateStandardClasses uses wrong attributes for undefined';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

for (var p in this);

assertEq(Object.getOwnPropertyDescriptor(this, "undefined").writable, false);

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
