// |reftest| skip-if(!xulRuntime.shell) -- needs detachArrayBuffer()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
"use strict"; // make test fail when limitation below is fixed

var gTestfile = 'element-setting-ToNumber-detaches.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 1001547;
var summary =
  "Don't assert assigning into memory detached while converting the value to " +
  "assign into a number";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Technically per current spec the element-sets should throw in strict mode,
// but we just silently do nothing for now, somewhat due to limitations of our
// internal MOP (which can't easily say "try this special behavior, else fall
// back on normal logic"), somewhat because it's consistent with current
// behavior (as of this test's addition) for out-of-bounds sets.

var ab = new ArrayBuffer(64);
var ta = new Uint32Array(ab);
ta[4] = { valueOf() { detachArrayBuffer(ab); return 5; } };

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
