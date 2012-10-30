function f (v, i) {
    var c = v[i];
    switch (c) {
    case 0:
        assertEq(v[i], 0);
        break;
    case 1:
        assertEq(v[i], 1);
        break;
    default:
        if (c == 0 || c == 1)
            assertEq(c, false);
    }
}

var v = [0, 0.0, 0.1, 1, 1.0, 1.1, null, undefined];
for (var i = 0; i < 100; i++)
    f(v, i % v.length);
