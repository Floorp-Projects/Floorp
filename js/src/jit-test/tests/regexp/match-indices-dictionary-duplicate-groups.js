// |jit-test| --enable-regexp-duplicate-named-groups; skip-if: getBuildConfiguration("wasi")||getBuildConfiguration('release_or_beta')
var s = "";
var input = "";
for (var i = 0; i < 500; ++i) {
    s += "((?<a" + i + ">a)?|(?<a" + i + ">A)?)?";
    s += "(?<b" + i + ">b)?";
    if (i % 2) {
      input += "a";
    } else {
      input += "A";
    }
}

try {
  var r = RegExp(s, "d");
  var e = r.exec(input);

  for (var i = 0; i < 500; i++) {
    if (i % 2) {
      assertEq(e.groups["a" + i], "a");
    } else {
      assertEq(e.groups["a" + i], "A");
    }
    assertEq(e.groups["b" + i], undefined);

    assertEq(e.indices.groups["a" + i][0], i)
    assertEq(e.indices.groups["a" + i][1], i + 1)
    assertEq(e.indices.groups["b" + i], undefined)
  }
} catch (err) {
  assertEq(err.message, "too much recursion");
}
