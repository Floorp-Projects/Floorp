
if (typeof TypedObject === "undefined")
    quit();

// Make sure some builtin TypedObject functions are given sensible names.
assertEq(TypedObject.ArrayType.name, "ArrayType");
assertEq(TypedObject.StructType.name, "StructType");
