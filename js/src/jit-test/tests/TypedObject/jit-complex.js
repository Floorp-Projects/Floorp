// Test that we can optimize stuff like line.target.x without
// creating an intermediate object.

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.warmup.trigger", 30);

var PointType = new TypedObject.StructType({x: TypedObject.float64,
                                            y: TypedObject.float64});
var LineType = new TypedObject.StructType({source: PointType,
                                           target: PointType});

function manhattanDistance(line) {
  return (Math.abs(line.target.x - line.source.x) +
          Math.abs(line.target.y - line.source.y));
}

function foo() {
  var N = 100;
  var points = [];
  var obj;
  var s;

  var fromAToB = new LineType({source: {x: 22, y: 44},
                               target: {x: 66, y: 88}});

  for (var i = 0; i < N; i++) {
    assertEq(manhattanDistance(fromAToB), 88);
  }
}

foo();
