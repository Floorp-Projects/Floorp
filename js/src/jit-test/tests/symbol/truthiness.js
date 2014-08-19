var sym = this.Symbol || (() => "truthy");

for (var i = 0; i < 9; i++)
    assertEq(sym() ? 1 : 0, 1, "symbols are truthy");

var a = [0, 0, 0, 0, 0, sym(), sym()];
var b = [];
function f(i, v) {
    b[i] = v ? "yes" : "no";
}
for (var i = 0; i < a.length; i++)
    f(i, a[i]);
assertEq(b[b.length - 3], "no");
assertEq(b[b.length - 2], "yes");
assertEq(b[b.length - 1], "yes");
