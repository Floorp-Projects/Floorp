
function assertArraysFirstEqual(a, b) {
    assertEq(a[0], b[0]);
}

function check(b) {
    var a = deserialize(serialize(b));
    assertArraysFirstEqual(a, b);
}

check(new Int8Array(1));
check(new Float64Array(1));
