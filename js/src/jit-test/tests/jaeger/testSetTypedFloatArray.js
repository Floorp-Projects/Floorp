// vim: set ts=8 sts=4 et sw=4 tw=99:

function testSetTypedFloat32Array(k) {
    var ar = new Float32Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 897 * 800 * 800 * 810 * 1923437;
    var t = k + 555;
    var L = ar[k+7] = t & 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3" + k;
    ar[k+3] = true;
    assertEq(ar[0] - 12.3 >= 0 &&
             ar[0] - 12.3 <= 0.0001, true);
    assertEq(ar[1], 500);
    assertEq(ar[2], 30);
    assertEq(ar[3], 1);
    assertEq(ar[4], 715525927453369300000);
    assertEq(ar[5], NaN);
    assertEq(ar[6], NaN);
    assertEq(ar[7], 1);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedFloat64Array(k) {
    var ar = new Float64Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 897 * 800 * 800 * 810 * 1923437;
    var t = k + 555;
    var L = ar[k+7] = t & 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3" + k;
    ar[k+3] = true;
    assertEq(ar[0] - 12.3 >= 0 &&
             ar[0] - 12.3 <= 0.0001, true);
    assertEq(ar[1], 500);
    assertEq(ar[2], 30);
    assertEq(ar[3], 1);
    assertEq(ar[4], 715525949998080000000);
    assertEq(ar[5], NaN);
    assertEq(ar[6], NaN);
    assertEq(ar[7], 1);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

for (var i = 0; i <= 10; i++) {
    testSetTypedFloat32Array(0);
    testSetTypedFloat64Array(0);
    if (i == 5)
        gc();
}

