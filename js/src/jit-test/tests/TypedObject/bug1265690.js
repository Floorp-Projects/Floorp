if (!('oomTest' in this) || !this.hasOwnProperty("TypedObject"))
    quit();
lfCodeBuffer = `
    ArrayType = TypedObject.ArrayType;
    var StructType = TypedObject.StructType;
    float32 = TypedObject.float32;
    Point = new ArrayType(float32, 3);
    var Line = new StructType({ Point });
    new ArrayType(Line, 3);
`;
loadFile(lfCodeBuffer);
function loadFile(lfVarx) {
    oomTest(function() { eval(lfVarx) });
}
