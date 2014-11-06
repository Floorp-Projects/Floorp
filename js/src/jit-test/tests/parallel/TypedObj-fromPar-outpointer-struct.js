// Test basic fromPar parallel execution using an out
// pointer to generate a struct return.

if (!getBuildConfiguration().parallelJS)
  quit();

load(libdir + "parallelarray-helpers.js")

var { ArrayType, StructType, uint32 } = TypedObject;

function test() {
  var L = minItemsTestingThreshold;

  var Matrix = uint32.array(L, 2);
  var matrix = new Matrix();
  for (var i = 0; i < L; i++)
    matrix[i][0] = i;

  var Point = new StructType({x: uint32, y: uint32});
  var Points = Point.array();

  assertParallelExecSucceeds(
    // FIXME Bug 983692 -- no where to pass `m` to
    function(m) Points.fromPar(matrix, function(p, i, c, out) { out.y = p[0]; }),
    function(points) {
      for (var i = 0; i < L; i++) {
        assertEq(matrix[i][0], i);
        assertEq(matrix[i][1], 0);
        assertEq(points[i].x, 0);
        assertEq(points[i].y, i);
      }
    });
}

test();

