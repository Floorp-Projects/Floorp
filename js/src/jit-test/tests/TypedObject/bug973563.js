// Test that empty sized structs don't trigger any assertion failures.
// Public domain.

if (!this.hasOwnProperty("TypedObject"))
  quit();

var PointType = new TypedObject.StructType({});
var LineType = new TypedObject.StructType({source: PointType, target: PointType});
var fromAToB = new LineType({source: {x: 22, y: 44}, target: {x: 66, y: 88}});
