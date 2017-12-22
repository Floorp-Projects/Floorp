// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-call-replacer-once.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary = "Call replacer function exactly once per value";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var factor = 1;
function replacer(k, v)
{
  if (k === "")
    return v;

  return v * ++factor;
}

var obj = { a: 1, b: 2, c: 3 };

assertEq(JSON.stringify(obj, replacer), '{"a":2,"b":6,"c":12}');
assertEq(factor, 4);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
