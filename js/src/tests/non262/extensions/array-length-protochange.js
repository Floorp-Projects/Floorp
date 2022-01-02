// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 548671;
var summary =
  "Don't use a shared-permanent inherited property to implement " +
  "[].length or (function(){}).length";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a = [1, 2, 3];
a.__proto__ = null;
reportCompare("length" in a, true, "length should be own property of array");
reportCompare(Object.hasOwnProperty.call(a, "length"), true,
              "length should be own property of array");
reportCompare(a.length, 3, "array length should be 3");

var a = [], b = [];
b.__proto__ = a;
reportCompare(b.hasOwnProperty("length"), true,
              "length should be own property of array");
b.length = 42;
reportCompare(b.length, 42, "should have mutated b's (own) length");
reportCompare(a.length, 0, "should not have mutated a's (own) length");


if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
