// Bug 991981. Check for various quirks when setting a field of a typed object
// during which set operation the underlying buffer is detached.

if (typeof TypedObject === "undefined")
  quit();

load(libdir + "asserts.js")

var StructType = TypedObject.StructType;
var uint32 = TypedObject.uint32;

function main()
{
  var Point = new StructType({ x: uint32, y: uint32 });
  var Line = new StructType({ from: Point, to: Point });

  var line = new Line();

  line.to = { x: 22, get y() { return 44; } };

  assertEq(line.to.x, 22);
  assertEq(line.to.y, 44);
}

main();
