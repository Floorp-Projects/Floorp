// |reftest| skip-if(!xulRuntime.shell) -- needs neuter()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "typedarray-copyWithin-arguments-neutering.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "%TypedArray.prototype.copyWithin shouldn't misbehave horribly if " +
  "index-argument conversion neuters the underlying ArrayBuffer";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testBegin(dataType)
{
  var ab = new ArrayBuffer(0x1000);

  var begin =
    {
      valueOf: function()
      {
        neuter(ab, dataType);
        return 0x800;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.copyWithin(0, begin, 0x1000);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "neutering should work for start weirdness");
}
testBegin("change-data");
testBegin("same-data");

function testEnd(dataType)
{
  var ab = new ArrayBuffer(0x1000);

  var end =
    {
      valueOf: function()
      {
        neuter(ab, dataType);
        return 0x1000;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.copyWithin(0, 0x800, end);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "neutering should work for start weirdness");
}
testEnd("change-data");
testEnd("same-data");

function testDest(dataType)
{
  var ab = new ArrayBuffer(0x1000);

  var dest =
    {
      valueOf: function()
      {
        neuter(ab, dataType);
        return 0;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.copyWithin(dest, 0x800, 0x1000);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "neutering should work for start weirdness");
}
testDest("change-data");
testDest("same-data");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
