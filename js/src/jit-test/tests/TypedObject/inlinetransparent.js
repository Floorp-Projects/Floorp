if (!this.hasOwnProperty("TypedObject"))
    quit();

var TO = TypedObject;

var PointType = new TO.StructType({x: TO.int32, y: TO.int32});
var LineType = new TO.StructType({from: PointType, to: PointType});

function testBasic(how) {
    var line = new LineType();
    var from = line.from;
    var to = line.to;
    TO.storage(to).buffer.expando = "hello";
    var dataview = new DataView(TO.storage(from).buffer);
    line.from.x = 12;
    line.to.x = 3;
    if (how == 1)
        minorgc();
    else if (how == 2)
	gc();
    assertEq(from.x, 12);
    assertEq(from.y, 0);
    assertEq(to.x, 3);
    assertEq(to.y, 0);
    assertEq(TO.storage(to).byteOffset, 8);
    dataview.setInt32(8, 10, true);
    assertEq(to.x, 10);
    assertEq(TO.storage(line).buffer.expando, "hello");
}
for (var i = 0; i < 5; i++)
    testBasic(0);
for (var i = 0; i < 5; i++)
    testBasic(1);
for (var i = 0; i < 5; i++)
    testBasic(2);
