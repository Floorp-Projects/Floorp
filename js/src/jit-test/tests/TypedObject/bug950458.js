if (!this.hasOwnProperty("TypedObject"))
  quit();

// Test for fuzz condition failure. Cause of the failure
// was that we were not adding a type barrier after the
// creation of derived typed objects. When run in --ion-eager
// mode, arr[i] (below) would yield a derived typed object
// with an empty type set, which would then fail sanity
// checking assertions.
//
// Public domain.

var N = 100;
var T = TypedObject;
var Point = new T.StructType({x: T.uint32, y: T.uint32, z: T.uint32});
var PointArray = Point.array();
function foo(arr) {
  var sum = 0;
  for (var i = 0; i < N; i++) {
    sum += arr[i].x + arr[i].y + arr[i].z;
  }
}
foo(new PointArray(N));
