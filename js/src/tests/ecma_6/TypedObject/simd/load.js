// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Our float32array will have 16 elements
const SIZE_ARRAY = 16;

// 1 float32 == 4 bytes
const SIZE_BYTES = SIZE_ARRAY * 4;

function MakeComparator(kind, arr) {
    var bpe = arr.BYTES_PER_ELEMENT;
    var uint8 = (bpe != 1) ? new Uint8Array(arr.buffer) : arr;

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
      default:
        assertEq(true, false, "unknown SIMD kind");
    }

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

    return {
        loadX: function(index) {
            var v = SIMD[kind].loadX(arr, index);
            assertEqX4(v, slice(index, 1));
        },

        loadXY: function(index) {
            var v = SIMD[kind].loadXY(arr, index);
            assertEqX4(v, slice(index, 2));
        },

        loadXYZ: function(index) {
            var v = SIMD[kind].loadXYZ(arr, index);
            assertEqX4(v, slice(index, 3));
        },

        load: function(index) {
            var v = SIMD[kind].load(arr, index);
            assertEqX4(v, slice(index, 4));
        }
    }
}

function testLoad(kind, TA) {
    for (var i = SIZE_ARRAY; i--;)
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
        assertThrowsInstanceOf(() => SIMD[kind].load(ta, -1), TypeError);

        // Valid and invalid reads
        var C = MakeComparator(kind, ta);
        var bpe = ta.BYTES_PER_ELEMENT;

        var lastValidArgLoadX   = (SIZE_BYTES - 4)  / bpe | 0;
        var lastValidArgLoadXY  = (SIZE_BYTES - 8)  / bpe | 0;
        var lastValidArgLoadXYZ = (SIZE_BYTES - 12) / bpe | 0;
        var lastValidArgLoad    = (SIZE_BYTES - 16) / bpe | 0;

        C.load(0);
        C.load(1);
        C.load(2);
        C.load(3);
        C.load(lastValidArgLoad);
        assertThrowsInstanceOf(() => SIMD[kind].load(ta, lastValidArgLoad + 1), TypeError);

        C.loadX(0);
        C.loadX(1);
        C.loadX(2);
        C.loadX(3);
        C.loadX(lastValidArgLoadX);
        assertThrowsInstanceOf(() => SIMD[kind].loadX(ta, lastValidArgLoadX + 1), TypeError);

        C.loadXY(0);
        C.loadXY(1);
        C.loadXY(2);
        C.loadXY(3);
        C.loadXY(lastValidArgLoadXY);
        assertThrowsInstanceOf(() => SIMD[kind].loadXY(ta, lastValidArgLoadXY + 1), TypeError);

        C.loadXYZ(0);
        C.loadXYZ(1);
        C.loadXYZ(2);
        C.loadXYZ(3);
        C.loadXYZ(lastValidArgLoadXYZ);
        assertThrowsInstanceOf(() => SIMD[kind].loadXYZ(ta, lastValidArgLoadXYZ + 1), TypeError);
    }

    // Test ToInt32 behavior
    var v = SIMD[kind].load(TA, 12.5);
    assertEqX4(v, [12, 13, 14, 15]);

    var obj = {
        valueOf: function() { return 12 }
    }
    var v = SIMD[kind].load(TA, obj);
    assertEqX4(v, [12, 13, 14, 15]);

    var obj = {
        valueOf: function() { throw new TypeError("i ain't a number"); }
    }
    assertThrowsInstanceOf(() => SIMD[kind].load(TA, obj), TypeError);
}

testLoad('float32x4', new Float32Array(SIZE_ARRAY));
testLoad('int32x4', new Int32Array(SIZE_ARRAY));

if (typeof reportCompare === "function")
    reportCompare(true, true);
