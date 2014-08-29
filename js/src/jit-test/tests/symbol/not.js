var sym = this.Symbol || (() => "truthy");

for (var i = 0; i < 9; i++)
    assertEq(!sym(), false, "symbols are truthy");

var a = [0, 0, 0, 0, 0, sym(), sym()];
var b = [];
function f(i, v) {
    b[i] = !v;
}
for (var i = 0; i < a.length; i++)
    f(i, a[i]);
assertEq(b[b.length - 3], true);
assertEq(b[b.length - 2], false);
assertEq(b[b.length - 1], false);
