// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1197097;
var summary = "JSON.stringify shouldn't use context-wide cycle detection";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr;

// Nested yet separate JSON.stringify is okay.
arr = [{}];
assertEq(JSON.stringify(arr, function(k, v) {
  assertEq(JSON.stringify(arr), "[{}]");
  return v;
}), "[{}]");

// SpiderMonkey censors cycles in array-joining.  This mechanism must not
// interfere with the cycle detection in JSON.stringify.
arr = [{
  toString: function() {
    var s = JSON.stringify(arr);
    assertEq(s, "[{}]");
    return s;
  }
}];
assertEq(arr.join(), "[{}]");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
