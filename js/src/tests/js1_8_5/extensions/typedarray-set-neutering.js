// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 983344;
var summary =
  "Uint8Array.prototype.set issues when this array changes during setting";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var ab1 = new ArrayBuffer(200);
var a1 = new Uint8Array(ab1);
var a1_2 = new Uint8Array(10);

var src1 = [ 10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             ];
Object.defineProperty(src1, 4, {
  get: function () {
    neuter(ab1, "change-data");
    gc();
    return 200;
  }
});

a1.set(src1);

// Not really needed
Array.reverse(a1_2);

var ab2 = new ArrayBuffer(200);
var a2 = new Uint8Array(ab2);
var a2_2 = new Uint8Array(10);

var src2 = [ 10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             10, 20, 30, 40,
             ];
Object.defineProperty(src2, 4, {
  get: function () {
    neuter(ab2, "same-data");
    gc();
    return 200;
  }
});

a2.set(src2);

// Not really needed
Array.reverse(a2_2);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
