function f(x,y,z) {
    var z;
    if (x) {
        if (y) {
            z = 0xfffffff;
        } else {
            z = 0xfffffff;
        }
        assertFloat32(z, false);
    } else {
        z = Math.fround(z);
        assertFloat32(z, true);
    }
    assertFloat32(z, false);
    return z;
}

function g(x,y,z) {
    var z;
    if (x) {
        if (y) {
            z = 3;
        } else {
            z = 6;
        }
        assertFloat32(z, false);
    } else {
        z = Math.fround(z);
        assertFloat32(z, true);
    }
    assertFloat32(z, true);
    return z;
}

setJitCompilerOption("ion.usecount.trigger", 50);

for (var n = 100; n--; ) {
    assertEq(f(0,1,2), 2);
    assertEq(f(0,0,2), 2);
    assertEq(f(1,0,2), 0xfffffff);
    assertEq(f(1,1,2), 0xfffffff);

    assertEq(g(0,1,2), 2);
    assertEq(g(0,0,2), 2);
    assertEq(g(1,0,2), 6);
    assertEq(g(1,1,2), 3);
}
