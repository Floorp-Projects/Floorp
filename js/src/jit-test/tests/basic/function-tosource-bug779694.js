// This test creates poorly compressible input, which tests certain paths in
// source code compression.
var x = "";
for (var i=0; i<400; ++i) {
    x += String.fromCharCode(i * 289);
}
var s = "'" + x + "'";
assertEq(Function("evt", s).toString(), "function anonymous(evt) {\n" + s + "\n}");
