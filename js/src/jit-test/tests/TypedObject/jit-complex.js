// Test that we can optimize stuff like line.target.x without
// creating an intermediate object.

if (!this.hasOwnProperty("Type"))
  quit();

var PointType = new StructType({x: float64,
                                y: float64});
var LineType = new StructType({source: PointType,
                               target: PointType});

function manhattenDistance(line) {
  return (Math.abs(line.target.x - line.source.x) +
          Math.abs(line.target.y - line.source.y));
}

function foo() {
  var N = 30000;
  var points = [];
  var obj;
  var s;

  var fromAToB = new LineType({source: {x: 22, y: 44},
                               target: {x: 66, y: 88}});

  for (var i = 0; i < N; i++)
    assertEq(manhattenDistance(fromAToB), 88);
}

foo();
