if (typeof TypedObject === "undefined")
  quit();

var {StructType, uint32, storage} = TypedObject;
var S = new StructType({f: uint32, g: uint32});
function main() {
  var s = new S({f: 22, g: 44});
  detachArrayBuffer(storage(s).buffer);
  print(storage(s).byteOffset);
}
try {
    main();
    assertEq(true, false);
} catch (e) {
    assertEq(e instanceof TypeError, true);
}
