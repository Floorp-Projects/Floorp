// Test basic mapPar parallel execution using an out
// pointer to generate a struct return.

if (!this.hasOwnProperty("TypedObject"))
  quit();

load(libdir + "parallelarray-helpers.js")

var { ArrayType, StructType, uint32 } = TypedObject;

function test() {
  var L = minItemsTestingThreshold;
  var Point = new StructType({x: uint32, y: uint32});
  var Points = Point.array();
  var points = new Points(L);
  for (var i = 0; i < L; i++)
    points[i].x = i;

  assertParallelExecSucceeds(
    // FIXME Bug 983692 -- no where to pass `m` to
    function(m) points.mapPar(function(p, i, c, out) { out.y = p.x; }),
    function(points2) {
      for (var i = 0; i < L; i++) {
        assertEq(points[i].x, i);
        assertEq(points[i].y, 0);
        assertEq(points2[i].x, 0);
        assertEq(points2[i].y, i);
      }
    });
}

test();

