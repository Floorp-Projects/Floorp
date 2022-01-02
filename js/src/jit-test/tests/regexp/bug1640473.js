// |jit-test| skip-if: getBuildConfiguration()['wasi']
var s = "";
var input = "";
for (var i = 0; i < 500; ++i) {
    s += "(?<a" + i + ">a)";
    s += "(?<b" + i + ">b)?";
    input += "a";
}

var r = RegExp(s);
var e = r.exec(input);

for (var i = 0; i < 500; i++) {
    assertEq(e.groups["a" + i], "a");
    assertEq(e.groups["b" + i], undefined);
}
