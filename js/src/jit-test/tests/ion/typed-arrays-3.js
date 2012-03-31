function testClampInt() {
    var values =   [-255, -254, -2, -1, 0, 1, 2, 128, 254, 255, 256, 300];
    var expected = [   0,    0,  0,  0, 0, 1, 2, 128, 254, 255, 255, 255];

    var arr = new Uint8ClampedArray(100);

    for (var i=0; i<10; i++) {
        for (var j=0; j<values.length; j++) {
            arr[1] = arr[j] = values[j];

            assertEq(arr[1], arr[j]);
            assertEq(arr[j], expected[j]);
        }
    }
}
testClampInt();

function testClampDouble() {
    var values =   [-255.1, -255.0, -2.1, -0, 0, 0.1, 123.4, 254.6, 255, 255.1, 255.9, 256, 1234.5,
                    NaN, -Infinity, Infinity];
    var expected = [     0,      0,    0,  0, 0, 0,     123,   255, 255,   255,   255, 255,    255,
                         0,      0,      255];

    var arr = new Uint8ClampedArray(100);

    for (var i=0; i<10; i++) {
        for (var j=0; j<values.length; j++) {
            arr[1] = arr[j] = values[j];

            assertEq(arr[1], arr[j]);
            assertEq(arr[j], expected[j]);
        }
    }
}
testClampDouble();

function testClampValue() {
    var values =   [null, 123, 256, 267.9, -123.3, undefined, Math, true, NaN, false];
    var expected = [   0, 123, 255,   255,      0,         0,    0,    1,   0,     0];

    var arr = new Uint8ClampedArray(100);

    for (var i=0; i<10; i++) {
        for (var j=0; j<values.length; j++) {
            arr[1] = arr[j] = values[j];

            assertEq(arr[1], arr[j]);
            assertEq(arr[j], expected[j]);
        }
    }
}
testClampValue();

function testString() {
    var arr = new Uint8ClampedArray(10);
    for (var i=0; i<60; i++) {
        arr[5] = ((i & 1) == 0) ? "123.5" : 33;
        if (i % 2 == 0)
            assertEq(arr[5], 124);
        else
            assertEq(arr[5], 33);
    }
}
//FIXME: enable this test again (bug 741114)
//testString();
