function assertArraysFirstEqual(a, b) {
    if (a.length != b.length) {}
}
function check(b) {
    var a = deserialize(serialize(b));
    assertArraysFirstEqual(a, b);
}
check(new Int8Array(1));
evaluate("check(['a', 'b']);");
