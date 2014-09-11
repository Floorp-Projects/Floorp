/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

// Test a single function (`bar`) being used with arrays that are all
// of known length, but not the same length.

setJitCompilerOption("ion.warmup.trigger", 30);

var N0 = 50;
var N1 = 100;
var N2 = 150;
var T = TypedObject;
var Array0 = T.uint32.array(N0);
var Array1 = T.uint32.array(N1);
var Array2 = T.uint32.array(N2);

function bar(array, i, v) {
  assertEq(array[i], v);
}

function foo() {
  var array0 = new Array0();
  var array1 = new Array1();
  var array2 = new Array2();

  for (var i = 0; i < N0; i++)
    array0[i] = i + 0;

  for (var i = 0; i < N1; i++)
    array1[i] = i + 1;

  for (var i = 0; i < N2; i++)
    array2[i] = i + 2;

  // get it primed up..
  for (var i = 0; i < N0; i++) {
    bar(array0, i, i);
    bar(array1, i, i + 1);
    bar(array2, i, i + 2);
  }

  // ...do some OOB accesses...
  for (var i = N0; i < N1; i++) {
    bar(array0, i, undefined);
    bar(array1, i, i + 1);
    bar(array2, i, i + 2);
  }

  // ...and some more.
  for (var i = N1; i < N2; i++) {
    bar(array0, i, undefined);
    bar(array1, i, undefined);
    bar(array2, i, i + 2);
  }
}

foo();
