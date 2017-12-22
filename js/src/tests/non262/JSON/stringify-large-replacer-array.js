// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-large-replacer-array.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 816033;
var summary = "JSON.stringify with a large replacer array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var replacer = [];
for (var i = 0; i < 4096; i++)
  replacer.push(i);

assertEq(JSON.stringify({ "foopy": "FAIL", "4093": 17 }, replacer), '{"4093":17}');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
