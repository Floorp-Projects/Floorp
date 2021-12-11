/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 858381;
var summary = "No-op array length redefinition";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr;

// initializedLength == capacity == length
// 6 == 6 == 6
arr = Object.defineProperty([0, 1, 2, 3, 4, 5], "length", { writable: false });
Object.defineProperty(arr, "length", { value: 6 });
Object.defineProperty(arr, "length", { writable: false });
Object.defineProperty(arr, "length", { configurable: false });
Object.defineProperty(arr, "length", { writable: false, configurable: false });
Object.defineProperty(arr, "length", { writable: false, value: 6 });
Object.defineProperty(arr, "length", { configurable: false, value: 6 });
Object.defineProperty(arr, "length", { writable: false, configurable: false, value: 6 });

// initializedLength == capacity < length
// 6 == 6 < 8
arr = Object.defineProperty([0, 1, 2, 3, 4, 5], "length", { value: 8, writable: false });
Object.defineProperty(arr, "length", { value: 8 });
Object.defineProperty(arr, "length", { writable: false });
Object.defineProperty(arr, "length", { configurable: false });
Object.defineProperty(arr, "length", { writable: false, configurable: false });
Object.defineProperty(arr, "length", { writable: false, value: 8 });
Object.defineProperty(arr, "length", { configurable: false, value: 8 });
Object.defineProperty(arr, "length", { writable: false, configurable: false, value: 8 });

// initializedLength < capacity == length
// 7 < 8 == 8
arr = Object.defineProperty([0, 1, 2, 3, 4, 5, 6, /* hole */, ], "length",
                            { value: 8, writable: false });
Object.defineProperty(arr, "length", { value: 8 });
Object.defineProperty(arr, "length", { writable: false });
Object.defineProperty(arr, "length", { configurable: false });
Object.defineProperty(arr, "length", { writable: false, configurable: false });
Object.defineProperty(arr, "length", { writable: false, value: 8 });
Object.defineProperty(arr, "length", { configurable: false, value: 8 });
Object.defineProperty(arr, "length", { writable: false, configurable: false, value: 8 });

// initializedLength < capacity < length
// 3 < 6 < 8
arr = Object.defineProperty([0, 1, 2], "length", { value: 8, writable: false });
Object.defineProperty(arr, "length", { value: 8 });
Object.defineProperty(arr, "length", { writable: false });
Object.defineProperty(arr, "length", { configurable: false });
Object.defineProperty(arr, "length", { writable: false, configurable: false });
Object.defineProperty(arr, "length", { writable: false, value: 8 });
Object.defineProperty(arr, "length", { configurable: false, value: 8 });
Object.defineProperty(arr, "length", { writable: false, configurable: false, value: 8 });

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
