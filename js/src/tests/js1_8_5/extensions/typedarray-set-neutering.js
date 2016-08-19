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

var ab = new ArrayBuffer(200);
var a = new Uint8Array(ab);
var a_2 = new Uint8Array(10);

var src = [ 10, 20, 30, 40,
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
Object.defineProperty(src, 4, {
  get: function () {
    detachArrayBuffer(ab);
    gc();
    return 200;
  }
});

a.set(src);

// Not really needed
Array.reverse(a_2);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
