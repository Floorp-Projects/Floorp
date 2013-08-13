// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData: check reflection on StructType objects';

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
