var T86 = TypedObject;
var StringStruct = new T86.StructType({
    f69: T86.string
});
var s68 = new StringStruct();
function writeString(o26, v91) {
  o26.f69 += v91;
}
for (var i69 = 0; i69 < 1000000; i69++)
  writeString(s68);
