// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))

var BUGNUMBER = 578700;
var summary = 'Byte-sized type conversion';

var T = TypedObject;

function check(results, ctor) {
  print("ctor = ", ctor.toSource());

  // check applying the ctor directly
  for (var i = 0; i < results.length; i++)
    assertEq(results[i][0], ctor(results[i][1]));

  // check writing and reading from a struct
  var S = new T.StructType({f: ctor});
  for (var i = 0; i < results.length; i++) {
    var s = new S({f: results[i][1]});
    assertEq(results[i][0], s.f);
  }
}

function runTests() {
  print(BUGNUMBER + ": " + summary);

  var int8results = [
    [22, 22],
    [-128, 128],
    [-1, 255],
    [-128, -128],
    [127, -129],
    [0x75, 0x7575],
    [-123, 0x7585]
  ];
  check(int8results, T.int8);

  var uint8results = [
    [22, 22],
    [128, 128],
    [255, 255],
    [0, 256],
    [128, -128],
    [127, -129],
    [129, 129],
    [0x75, 0x7575],
    [0x85, 0x7585]
  ];
  check(uint8results, T.uint8);

  var uint8clampedresults = [
    [22, 22],
    [128, 128],
    [255, 255],
    [0, -128],
    [0, -129],
    [129, 129],
    [255, 0x7575],
    [255, 0x7585]
  ];
  check(uint8clampedresults, T.uint8Clamped);

  if (typeof reportCompare === "function")
    reportCompare(true, true);

  print("Tests complete");
}

runTests();
