function f (x) {
    return x >> 0;
}

var table = [
    [NaN, 0],

    [Infinity, 0],
    [-Infinity, 0],
    [0, 0],
    [-0, 0],

    [15, 15],
    [-15, -15],

    [0x80000000, -0x80000000],
    [-0x80000000, -0x80000000],

    [0xffffffff, -1],
    [-0xffffffff, 1],

    [0x7fffffff, 0x7fffffff],
    [-0x7fffffff, -0x7fffffff]
]

for (var i = 0; i < table.length; i++) {    
    assertEq(f(table[i][0]), table[i][1]);
}
