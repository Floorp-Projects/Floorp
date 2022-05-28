// |jit-test| skip-if: true

// Common code for the ad-hack test cases.

function get(arr, loc, len) {
    let res = [];
    for ( let i=0; i < len; i++ ) {
        res.push(arr[loc+i]);
    }
    return res;
}

function getUnaligned(arr, width, loc, len) {
    assertEq(arr.constructor, Uint8Array);
    assertEq(width <= 4, true);
    let res = [];
    for ( let i=0; i < len; i++ ) {
        let x = 0;
        for ( let j=width-1; j >=0; j-- )
            x = (x << 8) | arr[loc+i*width+j];
        res.push(x);
    }
    return res;
}

function set(arr, loc, vals) {
    for ( let i=0; i < vals.length; i++ ) {
        if (arr instanceof BigInt64Array) {
            arr[loc+i] = BigInt(vals[i]);
        } else {
            arr[loc+i] = vals[i];
        }
    }
}

function setUnaligned(arr, width, loc, vals) {
    assertEq(arr.constructor, Uint8Array);
    assertEq(width <= 4, true);
    for ( let i=0; i < vals.length; i++ ) {
        let x = vals[i];
        for ( let j=0 ; j < width ; j++ ) {
            arr[loc+i*width + j] = x & 255;
            x >>= 8;
        }
    }
}

function equal(a, b) {
    return a === b || isNaN(a) && isNaN(b);
}

function upd(xs, at, val) {
    let ys = Array.from(xs);
    ys[at] = val;
    return ys;
}

// The following operations are not always generalized fully, they are just
// functional enough for the existing test cases to pass.

function sign_extend(n, bits) {
    if (bits < 32) {
        n = Number(n);
        return (n << (32 - bits)) >> (32 - bits);
    }
    if (typeof n == "bigint") {
        if (bits == 32)
            return Number(n & 0xFFFF_FFFFn) | 0;
        assertEq(bits, 64);
        n = (n & 0xFFFF_FFFF_FFFF_FFFFn)
        if (n > 0x7FFF_FFFF_FFFF_FFFFn)
            return n - 0x1_0000_0000_0000_0000n;
        return n;
    }
    assertEq(bits, 32);
    return n|0;
}

function zero_extend(n, bits) {
    if (bits < 32) {
        return n & ((1 << bits) - 1);
    }
    if (n < 0)
        n = 0x100000000 + n;
    return n;
}

function signed_saturate(z, bits) {
    let min = -(1 << (bits-1));
    if (z <= min) {
        return min;
    }
    let max = (1 << (bits-1)) - 1;
    if (z > max) {
        return max;
    }
    return z;
}

function unsigned_saturate(z, bits) {
    if (z <= 0) {
        return 0;
    }
    let max = (1 << bits) - 1;
    if (z > max) {
        return max;
    }
    return z;
}

function shl(count, width) {
    if (width == 64) {
        count = BigInt(count);
        return (v) => {
            v = BigInt(v);
            if (v < 0)
                v = (1n << 64n) + v;
            let r = (v << count) & ((1n << 64n) - 1n);
            if (r & (1n << 63n))
                r = -((1n << 64n) - r);
            return r;
        }
    } else {
        return (v) => {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (v << count) & mask;
        }
    }
}

function popcount(n) {
  n = n - ((n >> 1) & 0x55555555)
  n = (n & 0x33333333) + ((n >> 2) & 0x33333333)
  return ((n + (n >> 4) & 0xF0F0F0F) * 0x1010101) >> 24
}

function jsValueToWasmName(x) {
    if (typeof x == "number") {
        if (x == 0) return 1 / x < 0 ? "-0" : "0";
        if (isNaN(x)) return "+nan";
        if (!isFinite(x)) return (x < 0 ? "-" : "+") + "inf";
    }
    return x;
}

// For each input array, a set of arrays of the proper length for v128, with
// values in range but possibly of the wrong signedness (eg, for Int8Array, 128
// is in range but is really -128).  Also a unary operator `rectify` that
// transforms the value to the proper sign and bitwidth.

Int8Array.inputs = [iota(16).map((x) => (x+1) * (x % 3 == 0 ? -1 : 1)),
                    iota(16).map((x) => (x*2+3) * (x % 3 == 1 ? -1 : 1)),
                    [1,2,128,127,1,4,128,127,1,2,129,125,1,2,254,0],
                    [2,1,127,128,5,1,127,128,2,1,126,130,2,1,1,255],
                    iota(16).map((x) => ((x + 37) * 8 + 12) % 256),
                    iota(16).map((x) => ((x + 12) * 4 + 9) % 256)];
Int8Array.rectify = (x) => sign_extend(x,8);
Int8Array.layoutName = 'i8x16';

Uint8Array.inputs = Int8Array.inputs;
Uint8Array.rectify = (x) => zero_extend(x,8);
Uint8Array.layoutName = 'i8x16';

Int16Array.inputs = [iota(8).map((x) => (x+1) * (x % 3 == 0 ? -1 : 1)),
                     iota(8).map((x) => (x*2+3) * (x % 3 == 1 ? -1 : 1)),
                     [1,2,32768,32767,1,4,32768,32767],
                     [2,1,32767,32768,5,1,32767,32768],
                     [1,2,128,127,1,4,128,127].map((x) => (x << 8) + x*2),
                     [2,1,127,128,1,1,128,128].map((x) => (x << 8) + x*3)];
Int16Array.rectify = (x) => sign_extend(x,16);
Int16Array.layoutName = 'i16x8';

Uint16Array.inputs = Int16Array.inputs;
Uint16Array.rectify = (x) => zero_extend(x,16);
Uint16Array.layoutName = 'i16x8';

Int32Array.inputs = [iota(4).map((x) => (x+1) * (x % 3 == 0 ? -1 : 1)),
                     iota(4).map((x) => (x*2+3) * (x % 3 == 1 ? -1 : 1)),
                     [1,2,32768 << 16,32767 << 16],
                     [2,1,32767 << 16,32768 << 16],
                     [1,2,128,127].map((x) => (x << 24) + (x << 8) + x*3),
                     [2,1,127,128].map((x) => (x << 24) + (x << 8) + x*4)];
Int32Array.rectify = (x) => sign_extend(x,32);
Int32Array.layoutName = 'i32x4';

Uint32Array.inputs = Int32Array.inputs;
Uint32Array.rectify = (x) => zero_extend(x,32);
Uint32Array.layoutName = 'i32x4';

BigInt64Array.inputs = [[1,2],[2,1],[-1,-2],[-2,-1],[2n ** 32n, 2n ** 32n - 5n],
                        [(2n ** 38n) / 5n, (2n ** 41n) / 7n],
                        [-((2n ** 38n) / 5n), (2n ** 41n) / 7n]];
BigInt64Array.rectify = (x) => BigInt(x);
BigInt64Array.layoutName = 'i64x2';

Float32Array.inputs = [[1, -1, 1e10, -1e10],
                       [-1, -2, -1e10, 1e10],
                       [5.1, -1.1, -4.3, -0],
                       ...permute([1, -10, NaN, Infinity])];
Float32Array.rectify = (x) => Math.fround(x);
Float32Array.layoutName = 'f32x4';

Float64Array.inputs = Float32Array.inputs.map((x) => x.slice(0, 2))
Float64Array.rectify = (x) => x;
Float64Array.layoutName = 'f64x2';

// Tidy up all the inputs
for ( let A of [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, BigInt64Array,
                Float32Array, Float64Array]) {
    A.inputs = A.inputs.map((xs) => xs.map(A.rectify));
}
