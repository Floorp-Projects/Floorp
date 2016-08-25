const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array ];

// Ensure that when creating TypedArrays under JIT
// the sort() method works as expected (bug 1295034).
for (var ctor of constructors) {
  for (var _ of Array(1024)) {
    var testArray = new ctor(1024);
    testArray.sort();
  }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);