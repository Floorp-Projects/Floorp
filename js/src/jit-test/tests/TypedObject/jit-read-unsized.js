/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.warmup.trigger", 30);

var N = 100;
var T = TypedObject;
var Point = new T.StructType({x: T.uint32, y: T.uint32, z: T.uint32});
var PointArray = Point.array();

function bar(array, i, x, y, z) {
  assertEq(array[i].x, x);
  assertEq(array[i].y, y);
  assertEq(array[i].z, z);
}

function foo() {
  var array = new PointArray(N);
  for (var i = 0; i < N; i++) {
    array[i].x = i + 0;
    array[i].y = i + 1;
    array[i].z = i + 2;
  }

  // get it primed up..
  for (var i = 0; i < N; i++)
    bar(array, i, i, i + 1, i + 2);

  // ...do some OOB accesses...
  for (var i = 0; i < N; i++) {
    try {
      bar(array, N, undefined, undefined, undefined);
      assertEq(false, true);
    } catch(e) { }
  }

  // ...test again.
  for (var i = 0; i < N; i++)
    bar(array, i, i, i + 1, i + 2);
}

foo();
