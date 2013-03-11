load(libdir + "parallelarray-helpers.js");

function test() {
  // Test what happens if the length of the array is very short (i.e.,
  // less than the number of cores).  There used to be a bug in this
  // case that led to crashes or other undefined behavior.
  var makeadd1 = function (v) { return [v]; }
  compareAgainstArray(range(1, 3), "map", makeadd1);
}

test();
