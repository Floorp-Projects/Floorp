load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ 1 ]); },
                          "struct field descriptors require a valid name and type (got the number 1)");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { x: 1, y: 2 } ]); },
                          "struct field descriptors must contain one property (got the object ({x:1, y:2}) with 2 properties)");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { 1: 1 } ]); },
                         "the number 1 is not a valid name of struct field descriptors");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { "x": 1 } ]); },
                         "the number 1 is not a valid type of struct field descriptors for 'x' field");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { "x": ctypes.StructType("b") } ]); },
                         "struct field type must have defined and nonzero size (got ctypes.StructType(\"b\") for 'x' field)");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { "x": ctypes.int32_t, }, { "x": ctypes.int32_t } ]); },
                         "struct fields must have unique names, 'x' field appears twice");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [ { "x": ctypes.int32_t, } ])().addressOfField("z"); },
                         "ctypes.StructType(\"a\", [{ \"x\": ctypes.int32_t }]) does not have a field named 'z'");
}

if (typeof ctypes === "object")
  test();
