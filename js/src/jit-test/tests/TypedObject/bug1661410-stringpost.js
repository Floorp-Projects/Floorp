gczeal(2, 2000);

var tobj = TypedObject;
var StringStruct = new tobj.StructType({
    str: tobj.string
});
var s = new StringStruct();
function writeString(to, v91) {
  to.str += v91;
}
for (var i = 0; i < 9500; i++)
  writeString(s, 'x');
