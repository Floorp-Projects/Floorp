var v = [ -0x80000003, -0x80000002, -0x80000001, -0x80000000, -0x7fffffff, -0x7ffffffe, -0x7ffffffd,
          -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
          10, 11, 20, 21, 100, 101, 110, 111, 500,
           0x7ffffffd,  0x7ffffffe,  0x7fffffff,  0x80000000,  0x80000001,  0x80000002,  0x80000003];

function strcmp(x, y) {
    return Number(String(x) > String(y))
}

for (var i = 0; i < v.length; ++i) {
    for (var j = 0; j < v.length; ++j) {
            var builtin = String([v[i], v[j]].sort());
            var manual =  String([v[i], v[j]].sort(strcmp));
            assertEq(builtin, manual);
    }
}
