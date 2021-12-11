// Based on tests/ion/typed-arrays-1.js, but with string indexes
function testInt8() {
    var arr1 = new Int8Array(50);
    var arr2 = new Uint8Array(50);
    var arr3 = new Uint8ClampedArray(50);

    for (var i=0; i<arr1.length; i++) {
        arr1[i] = arr2[i] = arr3[i] = i * 8;
    }
    var res = 0;
    for (var i=0; i<arr1.length; i++) {
        res += arr1[i+""] + arr2[i+""] + arr3[i+""] + arr2["10".concat("")];
    }
    assertEq(res, 18334);
}
testInt8();

function testInt16() {
    var arr1 = new Int16Array(70);
    var arr2 = new Uint16Array(70);

    for (var i=0; i<arr1.length; i++) {
        arr1[i] = arr2[i] = i * 1000;
    }
    var res = 0;
    for (var i=0; i<arr1.length; i++) {
        res += arr1[i+""] + arr2[i+""] + arr2["1".concat("")] + arr1["3".concat("")];
    }
    assertEq(res, 2423024);
}
testInt16();

function testInt32() {
    var arr = new Int32Array(60);
    arr[0] = -50;
    for (var i=1; i<arr.length; i++) {
        arr[i] = arr[(i-1)+""] + arr["0".concat("")];
        ++arr[0];
    }
    assertEq(arr[(arr.length-1)+""], -1289);
}
testInt32();

function testUint32() {
    function sum(arr) {
        var res = 0;
        for (var i=0; i<arr.length; i++) {
            res += arr[i+""];
        }
        return res;
    }
    var arr = new Uint32Array(100);
    for (var i=0; i<arr.length; i++) {
        arr[i] = i;
    }

    // Compile sum() to read int32 values.
    assertEq(sum(arr), 4950);

    // Add a large uint32 so that the sum no longer fits in an
    // int32. sum() should be recompiled to return a double.
    arr[50] = 0xffffeeee;
    assertEq(sum(arr), 4294967826);
}
testUint32();

function testFloat() {
    var arr1 = new Float32Array(75);
    var arr2 = new Float64Array(75);
    arr1[0] = arr2[0] = Math.PI * 1234567.8;

    for (var i=1; i<75; i++) {
        arr1[i] = arr1[(i-1)+""] + arr1[0];
        arr2[i] = arr2[(i-1)+""] + arr2[0];
    }
    assertEq(arr1["74".concat("")] > 290888255, true);
    assertEq(arr1["74".concat("")] < 290888257, true);

    assertEq(arr2["74".concat("")] > 290888184, true);
    assertEq(arr2["74".concat("")] < 290888185, true);
}
testFloat();

function testCanonicalNaN() {
    // NaN values have to be canonicalized. Otherwise, malicious scripts could
    // construct arbitrary Value's (due to our NaN boxing Value representation).
    var buf = new ArrayBuffer(16);
    var uint32 = new Uint32Array(buf);
    var f64 = new Float64Array(buf);
    var f32 = new Float32Array(buf);

    // Evil: write a JSVAL_TYPE_OBJECT type tag...
    uint32[0] = 0xffffff87;
    uint32[1] = 0xffffff87;

    // Make sure this value is interpreted as a double.
    for (var i=0; i<3; i++) {
        assertEq(isNaN(f64["0".concat("")]), true);
        assertEq(isNaN(f32["0".concat("")]), true);
    }
}
testCanonicalNaN();

function testOutOfBounds() {
    var buf = new ArrayBuffer(16);
    var uint32 = new Uint32Array(buf);

    uint32[0] = 0;
    uint32[1] = 1;

    for (var i=0; i<3; i++) {
      assertEq(uint32["0".concat("")], 0);
      assertEq(uint32["1".concat("")], 1);
      assertEq(uint32["2".concat("")], 0);
      assertEq(uint32["17".concat("")], undefined);
    }
}
testOutOfBounds();

function testStrangeIndexes() {
    var buf = new ArrayBuffer(16);
    var uint32 = new Uint32Array(buf);

    uint32[0] = 0;
    uint32[1] = 1;

    indexes = ["0", "1", "2", "3", "17", "3.5", "NaN", "undefined", "null"];
    solutions = [0, 1, 0, 0, undefined, undefined, undefined, undefined, undefined];

    for (var i=0; i<indexes.length; i++) {
      assertEq(uint32[indexes[i]], solutions[i]);
    }
}
testStrangeIndexes();

