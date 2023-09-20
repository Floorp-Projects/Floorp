// |jit-test| skip-if: getBuildConfiguration()['wasi']
var s = "";
var input = "";
for (var i = 0; i < 500; ++i) {
    s += "(?<a" + i + ">a)";
    s += "(?<b" + i + ">b)?";
    input += "a";
}

var r = RegExp(s, "d");
var e = r.exec(input);

for (var i = 0; i < 500; i++) {
    assertEq(e.groups["a" + i], "a");
    assertEq(e.groups["b" + i], undefined);

    assertEq(e.indices.groups["a" + i][0], i)
    assertEq(e.indices.groups["a" + i][1], i + 1)
    assertEq(e.indices.groups["b" + i], undefined)
}
