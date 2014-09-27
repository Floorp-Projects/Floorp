if (!this.hasOwnProperty("TypedObject"))
    quit();

var TO = TypedObject;

var PointType = new TO.StructType({x: TO.float64, y: TO.float64, name:TO.string});
var LineType = new TO.StructType({from: PointType, to: PointType});

function testBasic(gc) {
    var line = new LineType();
    var from = line.from;
    var to = line.to;
    line.from.x = 12;
    line.from.name = "three";
    if (gc)
	minorgc();
    assertEq(to.name, "");
    assertEq(from.name, "three");
    assertEq(from.x, 12);
    assertEq(from.y, 0);
}
for (var i = 0; i < 5; i++)
    testBasic(false);
for (var i = 0; i < 5; i++)
    testBasic(true);
