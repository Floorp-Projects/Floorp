// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects: check reflection on StructType objects';

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var uint16 = TypedObject.uint16;
var uint32 = TypedObject.uint32;
var uint8Clamped = TypedObject.uint8Clamped;
var int8 = TypedObject.int8;
var int16 = TypedObject.int16;
var int32 = TypedObject.int32;
var float32 = TypedObject.float32;
var float64 = TypedObject.float64;

function runTests() {
    print(BUGNUMBER + ": " + summary);

    var S = new StructType({x: int32, y: uint8, z: float64});
    assertEq(S.__proto__, StructType.prototype);
    assertEq(S.prototype.__proto__, StructType.prototype.prototype);
    assertEq(S.toSource(), "StructType({x: int32, y: uint8, z: float64})");
    assertEq(S.variable, false);
    assertEq(S.byteLength, 16);
    assertEq(S.byteAlignment, 8);
    assertEq(S.fieldNames[0], "x");
    assertEq(S.fieldNames[1], "y");
    assertEq(S.fieldNames[2], "z");
    assertEq(S.fieldNames.length, 3);
    assertEq(S.fieldTypes.x, int32);
    assertEq(S.fieldTypes.y, uint8);
    assertEq(S.fieldTypes.z, float64);
    assertEq(S.fieldOffsets.x, 0);
    assertEq(S.fieldOffsets.y, 4);
    assertEq(S.fieldOffsets.z, 8);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
