/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "DataView-set-arguments-detaching.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "DataView.prototype.set* methods shouldn't misbehave horribly if " +
  "index-argument conversion detaches the ArrayBuffer being modified";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testIndex()
{
  var ab = new ArrayBuffer(0x1000);

  var dv = new DataView(ab);

  var start =
    {
      valueOf: function()
      {
        detachArrayBuffer(ab);
        gc();
        return 0xFFF;
      }
    };

  var ok = false;
  try
  {
    dv.setUint8(start, 0x42);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "should have thrown");
  assertEq(ab.byteLength, 0, "should have been detached correctly");
}
testIndex();

function testValue()
{
  var ab = new ArrayBuffer(0x100000);

  var dv = new DataView(ab);

  var value =
    {
      valueOf: function()
      {
        detachArrayBuffer(ab);
        gc();
        return 0x42;
      }
    };

  var ok = false;
  try
  {
    dv.setUint8(0xFFFFF, value);
  }
  catch (e)
  {
    ok = true;
  }
  assertEq(ok, true, "should have thrown");
  assertEq(ab.byteLength, 0, "should have been detached correctly");
}
testValue();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
