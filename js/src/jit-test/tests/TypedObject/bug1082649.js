if (typeof TypedObject === "undefined")
  quit();

var {StructType, uint32, storage} = TypedObject;
var S = new StructType({f: uint32, g: uint32});
function main(variant) {
  var s = new S({f: 22, g: 44});
  neuter(storage(s).buffer, variant);
  print(storage(s).byteOffset);
}
try {
    main("same-data");
    assertEq(true, false);
} catch (e) {
    assertEq(e instanceof TypeError, true);
}
