// |reftest| skip-if(!xulRuntime.shell) -- needs neuter()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "set-object-funky-length-neuters.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "%TypedArray.set(object with funky length property, numeric offset) " +
  "shouldn't misbehave if the funky length property neuters this typed array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var ctors = [Int8Array, Uint8Array, Uint8ClampedArray,
             Int16Array, Uint16Array,
             Int32Array, Uint32Array,
             Float32Array, Float64Array];
ctors.forEach(function(TypedArray) {
  ["change-data", "same-data"].forEach(function(dataHandling) {
    var buf = new ArrayBuffer(512 * 1024);
    var ta = new TypedArray(buf);

    var arraylike =
      {
        0: 17,
        1: 42,
        2: 3,
        3: 99,
        4: 37,
        5: 9,
        6: 72,
        7: 31,
        8: 22,
        9: 0,
        get length()
        {
          neuter(buf, dataHandling);
          return 10;
        }
      };

      var passed = false;
      try
      {
        ta.set(arraylike, 0x1234);
      }
      catch (e)
      {
        passed = true;
      }

      assertEq(passed, true);
  });
});

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
