// vim: set ts=4 sw=4 tw=99 et:

function testInt32Array(L) {
    var f = new Int32Array(8);
    assertEq(f[0], 0);
    assertEq(f[L], 0);
    assertEq(f[L+8], undefined);
    assertEq(f[8], undefined);
    f[0] = 12;
    f[L+1] = 13;
    f[2] = f[1];
    f[L+3] = 4294967295;
    f[L+4] = true;
    f[L+5] = L;
    assertEq(f[0], 12);
    assertEq(f[1], 13);
    assertEq(f[2], 13);
    assertEq(f[3], -1);
    assertEq(f[4], 1);
    assertEq(f[5], 0);
}

function testUint32Array(L) {
    var f = new Uint32Array(8);
    assertEq(f[0], 0);
    assertEq(f[L], 0);
    assertEq(f[L+8], undefined);
    assertEq(f[8], undefined);
    f[0] = 12;
    f[L+1] = 13;
    f[2] = f[1];
    f[L+3] = 4294967295;
    f[L+4] = true;
    f[L+5] = L;
    assertEq(f[0], 12);
    assertEq(f[1], 13);
    assertEq(f[2], 13);
    assertEq(f[3], 4294967295);
    assertEq(f[4], 1);
    assertEq(f[5], 0);
}

for (var i = 0; i < 10; i++) {
    //testInt32Array(0);
    testUint32Array(0);
    if (i == 5)
        gc();
}

