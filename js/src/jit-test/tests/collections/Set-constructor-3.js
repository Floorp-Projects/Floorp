// The argument to Set may contain a value multiple times. Duplicates are discarded.

assertEq(Set(["testing", "testing", 123]).size, 2);

var values = [undefined, null, false, NaN, 0, -0, 6.022e23, -Infinity, "", "xyzzy", {}, Math.sin];
for (let v of values) {
    var a = [v, {}, {}, {}, v, {}, v, v];
    var s = Set(a);
    assertEq(s.size, 5);
    assertEq(s.has(v), true);
}
