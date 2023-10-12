// |jit-test|

function f(x) {
    var x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10;
    var y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, y10;
    var z0, z1, z2, z3, z4, z5, z6, z7, z8, z9, z10;
    if (x == 0)
        return;
    f(x - 1);
}

f(1000);
