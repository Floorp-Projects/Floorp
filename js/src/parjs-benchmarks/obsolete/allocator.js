var nc = 30, maxCol = nc*3, cr,cg,cb;

load(libdir + "util.js");

function allocateSomeStuff() {
  return [{a: 1, b: 2}, {c: 3, d: 4}];
}

function computeSequentially() {
  result = [];
  for (var r = 0; r < ROWS; r++) {
    for (var c = 0; c < COLS; c++) {
      result.push(allocateSomeStuff());
    }
  }
  return result;
}

function computeParallel() {
  return new ParallelArray([ROWS, COLS], function(r, c) {
    return allocateSomeStuff();
  }).flatten();
}

var ROWS  = 1024;
var COLS  = 1024;
var DEPTH = 2;

benchmark("ALLOCATOR", 2, 6,
          computeSequentially, computeParallel);
