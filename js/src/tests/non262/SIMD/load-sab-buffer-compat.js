// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var { MakeComparator } = Helpers;

function testSharedArrayBufferCompat() {
    var TA = new Float32Array(new SharedArrayBuffer(16*4));
    for (var i = 0; i < 16; i++)
        TA[i] = i + 1;

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
        for (var kind of ['Int32x4', 'Uint32x4', 'Float32x4', 'Float64x2']) {
            var comp = MakeComparator(kind, ta);
            comp.load(0);
            comp.load1(0);
            comp.load2(0);
            comp.load3(0);

            comp.load(3);
            comp.load1(3);
            comp.load2(3);
            comp.load3(3);
        }

        assertThrowsInstanceOf(() => SIMD.Int32x4.load(ta, 1024), RangeError);
        assertThrowsInstanceOf(() => SIMD.Uint32x4.load(ta, 1024), RangeError);
        assertThrowsInstanceOf(() => SIMD.Float32x4.load(ta, 1024), RangeError);
        assertThrowsInstanceOf(() => SIMD.Float64x2.load(ta, 1024), RangeError);
    }
}

testSharedArrayBufferCompat();

if (typeof reportCompare === "function")
    reportCompare(true, true);
