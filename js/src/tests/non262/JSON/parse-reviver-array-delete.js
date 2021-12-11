// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'parse-reviver-array-delete.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 999999;
var summary = "JSON.parse with a reviver which elides array elements";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

/*
 * The reviver deletes all properties from the to-be-returned array.  Thus
 * stringification reveals properties on the prototype chain -- but there are
 * none, so this result is unsurprising.
 */
assertEq(JSON.parse('[1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0]',
                    function revive(k, v)
                    {
                      if (k === "")
                        return v;
                      return undefined;
                    }) + "",
         ",,,,,,,,,,,,,,,,,,,");

/*
 * Now let's try a reviver that deletes every property but a mega-huge one.
 */
var str = "[";
var expected = "";
var expected2 = "";
for (var i = 0; i < 2048; i++)
{
  str += "1,";
  if (i === 2047)
  {
    expected += "1";
    expected2 += "1";
  }
  if (i === 3)
    expected2 += "17";
  expected += ",";
  expected2 += ",";
}
str += "1]";

assertEq(JSON.parse(str,
                    function reviver(k, v)
                    {
                      if (k === "" || k === "2047")
                        return v;
                      return undefined;
                    }) + "",
         expected);


Array.prototype[3] = 17;

/* Now, with a property on the prototype chain, it'll show through. */
assertEq(JSON.parse('[1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0]',
                    function revive(k, v)
                    {
                      if (k === "")
                        return v;
                      return undefined;
                    }) + "",
         ",,,17,,,,,,,,,,,,,,,,");


/* And here too. */
assertEq(JSON.parse(str,
                    function reviver(k, v)
                    {
                      if (k === "" || k === "2047")
                        return v;
                      return undefined;
                    }) + "",
         expected2);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
