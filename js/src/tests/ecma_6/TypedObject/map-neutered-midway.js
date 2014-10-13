// |reftest| skip-if(!this.hasOwnProperty("TypedObject")||!xulRuntime.shell) -- needs TypedObject, neuter()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 991981;
var summary =
  "Behavior of mapping from an array neutered midway through mapping";

function mapOneDimArrayOfUint8(dataHandling)
{
  var FourByteArray = TypedObject.uint8.array(4);
  var FourByteArrayArray = FourByteArray.array(4);

  var buf = new ArrayBuffer(16);
  var arr = new FourByteArrayArray(buf);

  var count = 0;
  assertThrowsInstanceOf(function()
  {
    arr.map(function(v)
    {
      if (count++ > 0)
        neuter(buf, dataHandling);
      return new FourByteArray();
    });
  }, TypeError, "mapping of a neutered object worked?");
}

function runTests()
{
  print(BUGNUMBER + ": " + summary);

  mapOneDimArrayOfUint8("change-data");
  mapOneDimArrayOfUint8("same-data");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

runTests();
