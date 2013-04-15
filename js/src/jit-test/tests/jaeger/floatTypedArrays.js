// vim: set ts=8 sts=4 et sw=4 tw=99:

function testFloat32Array(L) {
    var f = new Float32Array(8);
    assertEq(f[0], 0);
    assertEq(f[L], 0);
    assertEq(f[L+8], undefined);
    assertEq(f[8], undefined);
    f[0] = 12;
    f[L+1] = 13.5;
    f[2] = f[1];
    f[L+3] = 4294967295;
    f[L+4] = true;
    f[L+5] = L;
    assertEq(f[0], 12);
    assertEq(f[1], 13.5);
    assertEq(f[2], 13.5);
    assertEq(f[3], 4294967296);
    assertEq(f[4], 1);
    assertEq(f[5], 0);
}

function testFloat64Array(L) {
    var f = new Float64Array(8);
    assertEq(f[0], 0);
    assertEq(f[L], 0);
    assertEq(f[L+8], undefined);
    assertEq(f[8], undefined);
    f[0] = 12;
    f[L+1] = 13.5;
    f[2] = f[1];
    f[L+3] = 4294967295;
    f[L+4] = true;
    f[L+5] = L;
    assertEq(f[0], 12);
    assertEq(f[1], 13.5);
    assertEq(f[2], 13.5);
    assertEq(f[3], 4294967295);
    assertEq(f[4], 1);
    assertEq(f[5], 0);
}

function testNaNCanonicalization() {
    var buf = new ArrayBuffer(128);

    var u8 = new Uint8Array(buf);
    for (var i = 0; i < 128; i++)
        u8[i] = 0xFF;

    var dblarr = new Float64Array(buf);
    var asstr = dblarr[0] + "";
    var asnum = dblarr[0] + 0.0;
    assertEq(asstr, "NaN");
    assertEq(asnum, NaN);
}

for (var i = 0; i < 10; i++) {
    //testFloat32Array(0);
    //testFloat64Array(0);
    testNaNCanonicalization();
    if (i == 5)
        gc();
}

