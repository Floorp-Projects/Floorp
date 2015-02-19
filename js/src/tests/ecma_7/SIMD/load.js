// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Our array for int32x4 and float32x4 will have 16 elements
const SIZE_32_ARRAY = 16;
const SIZE_64_ARRAY = 8;

const SIZE_BYTES = SIZE_32_ARRAY * 4;

function IsSharedTypedArray(arr) {
    return arr && arr.buffer && arr.buffer instanceof SharedArrayBuffer;
}

function MakeComparator(kind, arr, shared) {
    var bpe = arr.BYTES_PER_ELEMENT;
    var uint8 = (bpe != 1) ? (IsSharedTypedArray(arr) ? new SharedUint8Array(arr.buffer)
                                                      : new Uint8Array(arr.buffer))
                           : arr;

    // Size in bytes of a single element in the SIMD vector.
    var sizeOfLaneElem;
    // Typed array constructor corresponding to the SIMD kind.
    var typedArrayCtor;
    switch (kind) {
      case 'int32x4':
        sizeOfLaneElem = 4;
        typedArrayCtor = Int32Array;
        break;
      case 'float32x4':
        sizeOfLaneElem = 4;
        typedArrayCtor = Float32Array;
        break;
      case 'float64x2':
        sizeOfLaneElem = 8;
        typedArrayCtor = Float64Array;
        break;
      default:
        assertEq(true, false, "unknown SIMD kind");
    }
    var lanes = 16 / sizeOfLaneElem;
    // Reads (numElemToRead * sizeOfLaneElem) bytes in arr, and reinterprets
    // these bytes as a typed array equivalent to the typed SIMD vector.
    var slice = function(start, numElemToRead) {
        // Read enough bytes
        var startBytes = start * bpe;
        var endBytes = startBytes + numElemToRead * sizeOfLaneElem;
        var asArray = Array.prototype.slice.call(uint8, startBytes, endBytes);

        // If length is less than SIZE_BYTES bytes, fill with 0.
        // This is needed for loadX, loadXY, loadXYZ which do only partial
        // reads.
        for (var i = asArray.length; i < SIZE_BYTES; i++) asArray[i] = 0;
        assertEq(asArray.length, SIZE_BYTES);

        return new typedArrayCtor(new Uint8Array(asArray).buffer);
    }

    var assertFunc = (lanes == 2) ? assertEqX2 : assertEqX4;
    var type = SIMD[kind];
    return {
        loadX: function(index) {
            var v = type.loadX(arr, index);
            assertFunc(v, slice(index, 1));
        },

        loadXY: function(index) {
            if (lanes < 4)
                return;
            var v = type.loadXY(arr, index);
            assertFunc(v, slice(index, 2));
        },

       loadXYZ: function(index) {
           if (lanes < 4)
               return;
           var v = type.loadXYZ(arr, index);
           assertFunc(v, slice(index, 3));
        },

        load: function(index) {
           var v = type.load(arr, index);
           assertFunc(v, slice(index, 4));
        }
    }
}

function testLoad(kind, TA) {
    var lanes = TA.length / 4;
    for (var i = TA.length; i--;)
        TA[i] = i;

    for (var ta of [
                    new Uint8Array(TA.buffer),
                    new Int8Array(TA.buffer),
                    new Uint16Array(TA.buffer),
                    new Int16Array(TA.buffer),
                    new Uint32Array(TA.buffer),
                    new Int32Array(TA.buffer),
                    new Float32Array(TA.buffer),
                    new Float64Array(TA.buffer)
                   ])
    {
        // Invalid args
        assertThrowsInstanceOf(() => SIMD[kind].load(), TypeError);
        assertThrowsInstanceOf(() => SIMD[kind].load(ta), TypeError);
        assertThrowsInstanceOf(() => SIMD[kind].load("hello", 0), TypeError);
        assertThrowsInstanceOf(() => SIMD[kind].load(ta, -1), RangeError);

        // Valid and invalid reads
        var C = MakeComparator(kind, ta);
        var bpe = ta.BYTES_PER_ELEMENT;

        var lastValidArgLoadX   = (SIZE_BYTES - (lanes == 4 ? 4 : 8))  / bpe | 0;
        var lastValidArgLoadXY  = (SIZE_BYTES - 8)  / bpe | 0;
        var lastValidArgLoadXYZ = (SIZE_BYTES - 12) / bpe | 0;
        var lastValidArgLoad    = (SIZE_BYTES - 16) / bpe | 0;

        C.load(0);
        C.load(1);
        C.load(2);
        C.load(3);
        C.load(lastValidArgLoad);
        assertThrowsInstanceOf(() => SIMD[kind].load(ta, lastValidArgLoad + 1), RangeError);

        C.loadX(0);
        C.loadX(1);
        C.loadX(2);
        C.loadX(3);
        C.loadX(lastValidArgLoadX);
        assertThrowsInstanceOf(() => SIMD[kind].loadX(ta, lastValidArgLoadX + 1), RangeError);

        C.loadXY(0);
        C.loadXY(1);
        C.loadXY(2);
        C.loadXY(3);
        C.loadXY(lastValidArgLoadXY);

        C.loadXYZ(0);
        C.loadXYZ(1);
        C.loadXYZ(2);
        C.loadXYZ(3);
        C.loadXYZ(lastValidArgLoadXYZ);

        if (lanes >= 4) {
            assertThrowsInstanceOf(() => SIMD[kind].loadXY(ta, lastValidArgLoadXY + 1), RangeError);
            assertThrowsInstanceOf(() => SIMD[kind].loadXYZ(ta, lastValidArgLoadXYZ + 1), RangeError);
        }
    }

    if (lanes == 4) {
        // Test ToInt32 behavior
        var v = SIMD[kind].load(TA, 12.5);
        assertEqX4(v, [12, 13, 14, 15]);

        var obj = {
            valueOf: function() { return 12 }
        }
        var v = SIMD[kind].load(TA, obj);
        assertEqX4(v, [12, 13, 14, 15]);
    }

    var obj = {
        valueOf: function() { throw new TypeError("i ain't a number"); }
    }
    assertThrowsInstanceOf(() => SIMD[kind].load(TA, obj), TypeError);
}

function testSharedArrayBufferCompat() {
    if (!this.SharedArrayBuffer || !this.SharedFloat32Array || !this.Atomics)
        return;

    var TA = new SharedFloat32Array(16);
    for (var i = 0; i < 16; i++)
        TA[i] = i + 1;

    for (var ta of [
                    new SharedUint8Array(TA.buffer),
                    new SharedInt8Array(TA.buffer),
                    new SharedUint16Array(TA.buffer),
                    new SharedInt16Array(TA.buffer),
                    new SharedUint32Array(TA.buffer),
                    new SharedInt32Array(TA.buffer),
                    new SharedFloat32Array(TA.buffer),
                    new SharedFloat64Array(TA.buffer)
                   ])
    {
        for (var kind of ['int32x4', 'float32x4', 'float64x2']) {
            var comp = MakeComparator(kind, ta);
            comp.load(0);
            comp.loadX(0);
            comp.loadXY(0);
            comp.loadXYZ(0);

            comp.load(3);
            comp.loadX(3);
            comp.loadXY(3);
            comp.loadXYZ(3);
        }

        assertThrowsInstanceOf(() => SIMD.int32x4.load(ta, 1024), RangeError);
        assertThrowsInstanceOf(() => SIMD.float32x4.load(ta, 1024), RangeError);
        assertThrowsInstanceOf(() => SIMD.float64x2.load(ta, 1024), RangeError);
    }
}

testLoad('float32x4', new Float32Array(SIZE_32_ARRAY));
testLoad('float64x2', new Float64Array(SIZE_64_ARRAY));
testLoad('int32x4', new Int32Array(SIZE_32_ARRAY));
testSharedArrayBufferCompat();

if (typeof reportCompare === "function")
    reportCompare(true, true);
