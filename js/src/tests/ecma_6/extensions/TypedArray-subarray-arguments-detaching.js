// |reftest| skip-if(!xulRuntime.shell) -- needs detachArrayBuffer()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "TypedArray-subarray-arguments-detaching.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "%TypedArray.prototype.subarray shouldn't misbehave horribly if " +
  "index-argument conversion detaches the underlying ArrayBuffer";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testBegin()
{
  var ab = new ArrayBuffer(0x1000);

  var begin =
    {
      valueOf: function()
      {
        detachArrayBuffer(ab);
        return 0x800;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(begin);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "detaching should work for start weirdness");
}
testBegin();

function testBeginWithEnd()
{
  var ab = new ArrayBuffer(0x1000);

  var begin =
    {
      valueOf: function()
      {
        detachArrayBuffer(ab);
        return 0x800;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(begin, 0x1000);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "detaching should work for start weirdness");
}
testBeginWithEnd();

function testEnd()
{
  var ab = new ArrayBuffer(0x1000);

  var end =
    {
      valueOf: function()
      {
        detachArrayBuffer(ab);
        return 0x1000;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(0x800, end);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "start weirdness should have thrown");
  assertEq(ab.byteLength, 0, "detaching should work for start weirdness");
}
testEnd();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
