// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-replacer-with-array-indexes.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary =
  "Call the replacer function for array elements with stringified indexes";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [0, 1, 2, 3, 4];

var seenTopmost = false;
var index = 0;
function replacer()
{
  assertEq(arguments.length, 2);

  var key = arguments[0], value = arguments[1];

  // Topmost array: ignore replacer call.
  if (key === "")
  {
    assertEq(seenTopmost, false);
    seenTopmost = true;
    return value;
  }

  assertEq(seenTopmost, true);

  assertEq(typeof key, "string");
  assertEq(key === index, false);
  assertEq(key === index + "", true);

  assertEq(value, index);

  index++;

  assertEq(this, arr);

  return value;
}

assertEq(JSON.stringify(arr, replacer), '[0,1,2,3,4]');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
