if (!this.hasOwnProperty("TypedObject"))
  quit();

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var uint32 = TypedObject.uint32;
var ObjectType = TypedObject.Object;
function runTests() {
  (function DimensionLinkedToUndimension() {
    var FiveUintsA = uint32.array(5);
    var FiveUintsB = uint32.array(5);
    assertEq(true, 
	FiveUintsA.equivalent(FiveUintsB)
	);
  })();
  (function PrototypeHierarchy() {
    schedulegc(3);
    var Uint8s = uint8.array(5);
  })();
}

runTests();
