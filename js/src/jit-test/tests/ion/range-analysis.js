// |jit-test| no-jm
// Disable JM until it got investigated in Bug 843902.

// Only fails with Ion.
function add_xors_1() {
    var res = 0;
    var step = 4;
    for (var i = 0x7fffffff | 0; i >= (1 << step); i -= (i >> step)) {
        var x = i ^ (i << 1);
        res += (((x + x) + res + res) | 0);
    }
    return res;
}

var r1 = add_xors_1();
for (var i = 0; i < 100; i++) {
    var r2 = add_xors_1();
    assertEq(r2, r1);
}

// Only fails with JM
function add_xors_2() {
    var res = 0;
    var step = 4;
    for (var i = 0x7fffffff | 0; i >= (1 << step); i -= (i >> step)) {
        var x = i ^ (i << 1);
        res += ((x + x) + res + res) | 0;
    }
    return res;
}

var r1 = add_xors_2();
for (var i = 0; i < 100; i++) {
    var r2 = add_xors_2();
    assertEq(r1, r2);
}
