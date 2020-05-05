// Ad-hoc test cases used during development.  Generally these are ordered from
// easier toward harder.

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

function assertSame(got, expected) {
    assertEq(got.length, expected.length);
    for ( let i=0; i < got.length; i++ ) {
        let g = got[i];
        let e = expected[i];
        if (typeof g != typeof e) {
            if (typeof g == "bigint")
                e = BigInt(e);
            else if (typeof e == "bigint")
                g = BigInt(g);
        }
        assertEq(g, e);
    }
}

function iota(len) {
    let xs = [];
    for ( let i=0 ; i < len ; i++ )
        xs.push(i);
    return xs;
}

function cross(xs) {
    let results = [];
    for ( let x of xs )
        for ( let y of xs )
            results.push([x,y]);
    return results;
}

function eq(a, b) {
    return a === b || isNaN(a) && isNaN(b);
}

// Remove a value v from an array xs, comparing equal for NaN.
function remove(v, xs) {
    let result = [];
    for ( let w of xs ) {
        if (eq(v, w))
            continue;
        result.push(w);
    }
    return result;
}

function permute(xs) {
    if (xs.length == 1)
        return [xs];
    let results = [];
    for (let v of xs)
        for (let tail of permute(remove(v, xs)))
            results.push([v, ...tail]);
    return results;
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
        return (v) => {
            // This is only right for small values
            return BigInt(v << count);
        }
    } else {
        return (v) => {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (v << count) & mask;
        }
    }
}

function shr(count, width) {
    return (v) => {
        if (width == 64) {
            if (v < 0) {
                // This basically mirrors what the SIMD code does, so if there's
                // a bug there then there's a bug here too.  Seems OK though.
                let s = 0x1_0000_0000_0000_0000n + BigInt(v);
                let t = s / BigInt(1 << count);
                let u = BigInt((1 << count) - 1) * (2n ** BigInt(64-count));
                let w = t + u;
                return w - 0x1_0000_0000_0000_0000n;
            }
            return BigInt(v) / BigInt(1 << count);
        } else {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (sign_extend(v,8) >> count) & mask;
        }
    }
}

function shru(count, width) {
    if (width == 64) {
        return (v) => {
            if (v < 0) {
                v = 0x1_0000_0000_0000_0000n + BigInt(v);
            }
            return BigInt(v) / BigInt(1 << count);
        }
    } else {
        return (v) => {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (v >>> count) & mask;
        }
    }
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

Uint8Array.inputs = Int8Array.inputs;
Uint8Array.rectify = (x) => zero_extend(x,8);

Int16Array.inputs = [iota(8).map((x) => (x+1) * (x % 3 == 0 ? -1 : 1)),
                     iota(8).map((x) => (x*2+3) * (x % 3 == 1 ? -1 : 1)),
                     [1,2,32768,32767,1,4,32768,32767],
                     [2,1,32767,32768,5,1,32767,32768],
                     [1,2,128,127,1,4,128,127].map((x) => (x << 8) + x*2),
                     [2,1,127,128,1,1,128,128].map((x) => (x << 8) + x*3)];
Int16Array.rectify = (x) => sign_extend(x,16);

Uint16Array.inputs = Int16Array.inputs;
Uint16Array.rectify = (x) => zero_extend(x,16);

Int32Array.inputs = [iota(4).map((x) => (x+1) * (x % 3 == 0 ? -1 : 1)),
                     iota(4).map((x) => (x*2+3) * (x % 3 == 1 ? -1 : 1)),
                     [1,2,32768 << 16,32767 << 16],
                     [2,1,32767 << 16,32768 << 16],
                     [1,2,128,127].map((x) => (x << 24) + (x << 8) + x*3),
                     [2,1,127,128].map((x) => (x << 24) + (x << 8) + x*4)];
Int32Array.rectify = (x) => sign_extend(x,32);

Uint32Array.inputs = Int32Array.inputs;
Uint32Array.rectify = (x) => zero_extend(x,32);

BigInt64Array.inputs = [[1,2],[2,1],[-1,-2],[-2,-1],[2n ** 32n, 2n ** 32n - 5n]];
BigInt64Array.rectify = (x) => BigInt(x);

Float32Array.inputs = [[1, -1, 1e10, -1e10],
                       [-1, -2, -1e10, 1e10],
                       ...permute([1, -10, NaN, Infinity])];
Float32Array.rectify = (x) => Math.fround(x);

Float64Array.inputs = Float32Array.inputs;
Float64Array.rectify = (x) => x;

// Tidy up all the inputs
for ( let A of [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, BigInt64Array,
                Float32Array, Float64Array]) {
    A.inputs = A.inputs.map((xs) => xs.map(A.rectify));
}

// v128.store
// v128.const
for ( let offset of [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "f") (param $loc i32)
      (v128.store offset=${offset} (local.get $loc) (v128.const i32x4 ${1+offset} 2 3 ${4+offset*2}))))`);
    var mem8 = new Uint8Array(ins.exports.mem.buffer);
    ins.exports.f(160);
    assertSame(getUnaligned(mem8, 4, 160 + offset, 4), [1+offset, 2, 3, 4+offset*2]);

    // OOB write should trap
    assertErrorMessage(() => ins.exports.f(65536-15),
                       WebAssembly.RuntimeError,
                       /index out of bounds/)

    // Ensure that OOB writes don't write anything.
    let start = 65536 - 15 + offset;
    let legalBytes = 65536 - start;
    assertSame(get(mem8, start, legalBytes), iota(legalBytes).map((_) => 0));
}

// v128.load
// v128.store
// temp register
for ( let offset of [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "copy") (param $dest i32) (param $src i32)
      (v128.store (local.get $dest) (v128.load offset=${offset} (local.get $src)))))`);
    var mem32 = new Uint32Array(ins.exports.mem.buffer);
    var mem8 = new Uint8Array(ins.exports.mem.buffer);
    setUnaligned(mem8, 4, 4*4 + offset, [8+offset, 10, 12, 14+offset*2]);
    ins.exports.copy(40*4, 4*4);
    assertSame(get(mem32, 40, 4), [8+offset, 10, 12, 14+offset*2]);
    assertErrorMessage(() => ins.exports.copy(40*4, 65536-15),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

// call [with register params]
// parameters [in registers]
// return [with register values]
// locals
//
// local.get
// local.set
// v128.const
// v128.store
var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $g (param $param v128) (result v128)
      (local $tmp v128)
      (local.set $tmp (local.get $param))
      (local.get $tmp))
    (func (export "f")
      (v128.store (i32.const 160) (call $g (v128.const i32x4 1 2 3 4)))))`);
var mem = new Uint32Array(ins.exports.mem.buffer);
ins.exports.f();
assertSame(get(mem, 40, 4), [1, 2, 3, 4]);

// Locals that end up on the stack.  Try to create unaligned placement (in the
// baseline compiler anyway) by inserting i32 locals before or after and
// inbetween the v128 ones and by having so many locals that we run out of
// registers.

var nlocals = 64;
for ( let start of [0, 1]) {
    let decl = "";
    let set = "";
    let sum = "(v128.const i32x4 0 0 0 0)";
    var res = [0,0,0,0];
    var locno = start;
    for ( let i=start ; i < start + nlocals ; i++ ) {
        decl += "(local v128) ";
        set += `(local.set ${locno} (v128.const i32x4 ${i} ${i+1} ${i+2} ${i+3})) `;
        sum = `(i32x4.add ${sum} (local.get ${locno}))`;
        locno++;
        res[0] += i;
        res[1] += i+1;
        res[2] += i+2;
        res[3] += i+3;
        if ((i % 5) == 3) {
            decl += "(local i32) ";
            locno++;
        }
    }
    if (start)
        decl = "(local i32) " + decl;
    else
        decl += "(local i32) ";
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $g (result v128)
      ${decl}
      ${set}
      ${sum})
    (func (export "f")
      (v128.store (i32.const 160) (call $g))))`);

    var mem = new Uint32Array(ins.exports.mem.buffer);
    ins.exports.f();
    assertSame(get(mem, 40, 4), res);
}

// Ditto parameters.  This is like the case above but values are passed rather
// than set.

var nlocals = 64;
for ( let start of [0, 1]) {
    let decl = "";
    let pass = "";
    let sum = "(v128.const i32x4 0 0 0 0)";
    var res = [0,0,0,0];
    var locno = start;
    for ( let i=start ; i < start + nlocals ; i++ ) {
        decl += "(param v128) ";
        pass += `(v128.const i32x4 ${i} ${i+1} ${i+2} ${i+3}) `;
        sum = `(i32x4.add ${sum} (local.get ${locno}))`;
        locno++;
        res[0] += i;
        res[1] += i+1;
        res[2] += i+2;
        res[3] += i+3;
        if ((i % 5) == 3) {
            decl += "(param i32) ";
            pass += "(i32.const 0) ";
            locno++;
        }
    }
    if (start) {
        decl = "(param i32) " + decl;
        pass = "(i32.const 0) " + pass;
    } else {
        decl += "(param i32) ";
        pass += "(i32.const 0) ";
    }
    var txt = `
  (module
    (memory (export "mem") 1 1)
    (func $g ${decl} (result v128)
      ${sum})
    (func (export "f")
      (v128.store (i32.const 160) (call $g ${pass}))))`;
    var ins = wasmEvalText(txt);

    var mem = new Uint32Array(ins.exports.mem.buffer);
    ins.exports.f();
    assertSame(get(mem, 40, 4), res);
}

// Simple binary operators.  Place parameters in memory at offsets 16 and 32,
// read the result at offset 0.

function insAndMemBinop(op, memtype, resultmemtype) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "run")
      (v128.store (i32.const 0)
        (call $doit (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func $doit (param $a v128) (param $b v128) (result v128)
      (${op} (local.get $a) (local.get $b))))`);
    var mem = new memtype(ins.exports.mem.buffer);
    var resultmem = !resultmemtype || memtype == resultmemtype ? mem : new resultmemtype(ins.exports.mem.buffer);
    return [ins, mem, resultmem];
}

function add(bits) { return (x, y) => sign_extend(x+y, bits) }
function add64(x, y) { return sign_extend(BigInt(x)+BigInt(y), 64) }
function sub(bits) { return (x, y) => sign_extend(x-y, bits) }
function sub64(x, y) { return sign_extend(BigInt(x)-BigInt(y), 64) }
// Even 32-bit multiply can overflow a Number, so always use BigInt
function mul(bits) { return (x, y) => sign_extend(BigInt(x)*BigInt(y), bits) }
function div(x, y) { return x/y }
function min(x, y) { return x < y ? x : y }
function max(x, y) { return x > y ? x : y }
function and(x, y) { return zero_extend(x&y, 8) }
function or(x, y) { return zero_extend(x|y, 8) }
function xor(x, y) { return zero_extend(x^y, 8) }
function andnot(x, y) { return zero_extend(x&~y, 8) }
function avgr(x, y) { return (x + y + 1) >> 1; }
function eq(truth) { return (x,y) => x==y ? truth : 0 }
function ne(truth) { return (x,y) => x!=y ? truth : 0 }
function lt(truth) { return (x, y) => x < y ? truth : 0 }
function gt(truth) { return (x, y) => x > y ? truth : 0 }
function le(truth) { return (x, y) => x <= y ? truth : 0 }
function ge(truth) { return (x, y) => x >= y ? truth : 0 }

function fadd(x, y) { return Math.fround(x+y) }
function fsub(x, y) { return Math.fround(x-y) }
function fmul(x, y) { return Math.fround(x*y) }
function fdiv(x, y) { return Math.fround(x/y) }

function dadd(x, y) { return x+y }
function dsub(x, y) { return x-y }
function dmul(x, y) { return x*y }
function ddiv(x, y) { return x/y }

function op_sat_s(bits, op) {
    return (x, y) => {
        return signed_saturate(op(sign_extend(x, bits),
                                  sign_extend(y, bits)),
                               bits);
    }
}

function op_sat_u(bits, op) {
    return (x, y) => {
        return unsigned_saturate(op(zero_extend(x, bits),
                                    zero_extend(y, bits)),
                                 bits);
    }
}

function add_sat_s(bits) {
    return op_sat_s(bits, (x,y) => x+y);
}
function sub_sat_s(bits) {
    return op_sat_s(bits, (x,y) => x-y);
}
function add_sat_u(bits) {
    return op_sat_u(bits, (x,y) => x+y);
}
function sub_sat_u(bits) {
    return op_sat_u(bits, (x,y) => x-y);
}

function max_s(bits) {
    return (x, y) => {
        return sign_extend(max(sign_extend(x, bits),
                               sign_extend(y, bits)),
                           bits);
    }
}

function min_s(bits) {
    return (x, y) => {
        return sign_extend(min(sign_extend(x, bits),
                               sign_extend(y, bits)),
                           bits);
    }
}

function max_u(bits) {
    return (x, y) => {
        return max(zero_extend(x, bits),
                   zero_extend(y, bits));
    }
}

function min_u(bits) {
    return (x, y) => {
        return min(zero_extend(x, bits),
                   zero_extend(y, bits));
    }
}

assertEq(max_s(8)(1, 2), 2);
assertEq(max_s(8)(1, 128), 1);
assertEq(min_s(8)(1, 2), 1);
assertEq(min_s(8)(1, 128), -128);
assertEq(max_u(8)(1, 2), 2);
assertEq(max_u(8)(1, 128), 128);
assertEq(min_u(8)(1, 2), 1);
assertEq(min_u(8)(1, 128), 1);

for ( let [op, memtype, rop, resultmemtype] of
      [['i8x16.add', Int8Array, add(8)],
       ['i16x8.add', Int16Array, add(16)],
       ['i32x4.add', Int32Array, add(32)],
       ['i64x2.add', BigInt64Array, add64],
       ['i8x16.sub', Int8Array, sub(8)],
       ['i16x8.sub', Int16Array, sub(16)],
       ['i32x4.sub', Int32Array, sub(32)],
       ['i64x2.sub', BigInt64Array, sub64],
       ['i8x16.add_saturate_s', Int8Array, add_sat_s(8)],
       ['i8x16.add_saturate_u', Uint8Array, add_sat_u(8)],
       ['i16x8.add_saturate_s', Int16Array, add_sat_s(16)],
       ['i16x8.add_saturate_u', Uint16Array, add_sat_u(16)],
       ['i8x16.sub_saturate_s', Int8Array, sub_sat_s(8)],
       ['i8x16.sub_saturate_u', Uint8Array, sub_sat_u(8)],
       ['i16x8.sub_saturate_s', Int16Array, sub_sat_s(16)],
       ['i16x8.sub_saturate_u', Uint16Array, sub_sat_u(16)],
       ['i16x8.mul', Int16Array, mul(16)],
       ['i32x4.mul', Int32Array, mul(32)],
       ['i64x2.mul', BigInt64Array, mul(64)],
       ['i8x16.avgr_u', Uint8Array, avgr],
       ['i16x8.avgr_u', Uint16Array, avgr],
       ['i8x16.max_s', Int8Array, max_s(8)],
       ['i8x16.max_u', Uint8Array, max_u(8)],
       ['i8x16.min_s', Int8Array, min_s(8)],
       ['i8x16.min_u', Uint8Array, min_u(8)],
       ['i16x8.max_s', Int16Array, max_s(16)],
       ['i16x8.max_u', Uint16Array, max_u(16)],
       ['i16x8.min_s', Int16Array, min_s(16)],
       ['i16x8.min_u', Uint16Array, min_u(16)],
       ['i32x4.max_s', Int32Array, max_s(32)],
       ['i32x4.max_u', Uint32Array, max_u(32)],
       ['i32x4.min_s', Int32Array, min_s(32)],
       ['i32x4.min_u', Uint32Array, min_u(32)],
       ['v128.and', Uint8Array, and],
       ['v128.or', Uint8Array, or],
       ['v128.xor', Uint8Array, xor],
       ['v128.andnot', Uint8Array, andnot],
       ['f32x4.add', Float32Array, fadd],
       ['f32x4.sub', Float32Array, fsub],
       ['f32x4.mul', Float32Array, fmul],
       ['f32x4.div', Float32Array, fdiv],
       ['f32x4.min', Float32Array, min],
       ['f32x4.max', Float32Array, max],
       ['f64x2.add', Float64Array, dadd],
       ['f64x2.sub', Float64Array, dsub],
       ['f64x2.mul', Float64Array, dmul],
       ['f64x2.div', Float64Array, ddiv],
       ['f64x2.min', Float64Array, min],
       ['f64x2.max', Float64Array, max],
       ['i8x16.eq', Int8Array, eq(-1)],
       ['i8x16.ne', Int8Array, ne(-1)],
       ['i8x16.lt_s', Int8Array, lt(-1)],
       ['i8x16.gt_s', Int8Array, gt(-1)],
       ['i8x16.le_s', Int8Array, le(-1)],
       ['i8x16.ge_s', Int8Array, ge(-1)],
       ['i8x16.gt_u', Uint8Array, gt(0xFF)],
       ['i8x16.ge_u', Uint8Array, ge(0xFF)],
       ['i8x16.lt_u', Uint8Array, lt(0xFF)],
       ['i8x16.le_u', Uint8Array, le(0xFF)],
       ['i16x8.eq', Int16Array, eq(-1)],
       ['i16x8.ne', Int16Array, ne(-1)],
       ['i16x8.lt_s', Int16Array, lt(-1)],
       ['i16x8.gt_s', Int16Array, gt(-1)],
       ['i16x8.le_s', Int16Array, le(-1)],
       ['i16x8.ge_s', Int16Array, ge(-1)],
       ['i16x8.gt_u', Uint16Array, gt(0xFFFF)],
       ['i16x8.ge_u', Uint16Array, ge(0xFFFF)],
       ['i16x8.lt_u', Uint16Array, lt(0xFFFF)],
       ['i16x8.le_u', Uint16Array, le(0xFFFF)],
       ['i32x4.eq', Int32Array, eq(-1)],
       ['i32x4.ne', Int32Array, ne(-1)],
       ['i32x4.lt_s', Int32Array, lt(-1)],
       ['i32x4.gt_s', Int32Array, gt(-1)],
       ['i32x4.le_s', Int32Array, le(-1)],
       ['i32x4.ge_s', Int32Array, ge(-1)],
       ['i32x4.gt_u', Uint32Array, gt(0xFFFFFFFF)],
       ['i32x4.ge_u', Uint32Array, ge(0xFFFFFFFF)],
       ['i32x4.lt_u', Uint32Array, lt(0xFFFFFFFF)],
       ['i32x4.le_u', Uint32Array, le(0xFFFFFFFF)],
       ['f32x4.eq', Float32Array, eq(-1), Int32Array],
       ['f32x4.ne', Float32Array, ne(-1), Int32Array],
       ['f32x4.lt', Float32Array, lt(-1), Int32Array],
       ['f32x4.gt', Float32Array, gt(-1), Int32Array],
       ['f32x4.le', Float32Array, le(-1), Int32Array],
       ['f32x4.ge', Float32Array, ge(-1), Int32Array],
       ['f64x2.eq', Float64Array, eq(-1), BigInt64Array],
       ['f64x2.ne', Float64Array, ne(-1), BigInt64Array],
       ['f64x2.lt', Float64Array, lt(-1), BigInt64Array],
       ['f64x2.gt', Float64Array, gt(-1), BigInt64Array],
       ['f64x2.le', Float64Array, le(-1), BigInt64Array],
       ['f64x2.ge', Float64Array, ge(-1), BigInt64Array],
      ])
{
    let [ins, mem, resultmem] = insAndMemBinop(op, memtype, resultmemtype);
    let len = 16/memtype.BYTES_PER_ELEMENT;
    let xs = iota(len);

    function testIt(a,b) {
        let r = xs.map((i) => rop(a[i], b[i]));
        set(mem, len, a);
        set(mem, len*2, b);
        ins.exports.run();
        assertSame(get(resultmem, 0, len), r);
    }

    for (let [a,b] of cross(memtype.inputs))
        testIt(a,b);
}

// Splat

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "splat_i8x16") (param $src i32)
      (v128.store (i32.const 0) (i8x16.splat (local.get $src))))
    (func (export "splat_i16x8") (param $src i32)
      (v128.store (i32.const 0) (i16x8.splat (local.get $src))))
    (func (export "splat_i32x4") (param $src i32)
      (v128.store (i32.const 0) (i32x4.splat (local.get $src))))
    (func (export "splat_i64x2") (param $src i64)
      (v128.store (i32.const 0) (i64x2.splat (local.get $src))))
    (func (export "splat_f32x4") (param $src f32)
      (v128.store (i32.const 0) (f32x4.splat (local.get $src))))
    (func (export "splat_f64x2") (param $src f64)
      (v128.store (i32.const 0) (f64x2.splat (local.get $src))))
)`);

ins.exports.splat_i8x16(3);
var mem8 = new Uint8Array(ins.exports.mem.buffer);
assertSame(get(mem8, 0, 16), [3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3]);

ins.exports.splat_i16x8(976);
var mem16 = new Uint16Array(ins.exports.mem.buffer);
assertSame(get(mem16, 0, 8), [976, 976, 976, 976, 976, 976, 976, 976]);

ins.exports.splat_i32x4(147812);
var mem32 = new Uint32Array(ins.exports.mem.buffer);
assertSame(get(mem32, 0, 4), [147812, 147812, 147812, 147812]);

ins.exports.splat_i64x2(147812n);
var mem64 = new BigInt64Array(ins.exports.mem.buffer);
assertSame(get(mem64, 0, 2), [147812, 147812]);

ins.exports.splat_f32x4(147812.5);
var memf32 = new Float32Array(ins.exports.mem.buffer);
assertSame(get(memf32, 0, 4), [147812.5, 147812.5, 147812.5, 147812.5]);

ins.exports.splat_f64x2(147812.5);
var memf64 = new Float64Array(ins.exports.mem.buffer);
assertSame(get(memf64, 0, 2), [147812.5, 147812.5]);

// Simple unary operators.  Place parameter in memory at offset 16,
// read the result at offset 0.

function insAndMemUnop(op, memtype, resultmemtype) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "run")
      (v128.store (i32.const 0)
        (call $doit (v128.load (i32.const 16)))))
    (func $doit (param $a v128) (result v128)
      (${op} (local.get $a))))`);
    var mem = new memtype(ins.exports.mem.buffer);
    var resultmem = !resultmemtype || memtype == resultmemtype ? mem : new resultmemtype(ins.exports.mem.buffer);
    return [ins, mem, resultmem];
}

function ineg(bits) { return (a) => sign_extend(!a ? a : -a,bits) }
function iabs(bits) { return (a) => zero_extend(a < 0 ? -a : a, bits) }
function fneg(a) { return -a }
function fabs(a) { return Math.abs(a) }
function fsqrt(a) { return Math.fround(Math.sqrt(Math.fround(a))) }
function sqrt(a) { return Math.sqrt(Math.fround(a)) }
function bitnot(a) { return (~a) & 255 }

for ( let [op, memtype, rop, resultmemtype] of
      [['i8x16.neg', Int8Array, ineg(8)],
       ['i16x8.neg', Int16Array, ineg(16)],
       ['i32x4.neg', Int32Array, ineg(32)],
       ['i64x2.neg', BigInt64Array, ineg(64)],
       ['i8x16.abs', Int8Array, iabs(8), Uint8Array],
       ['i16x8.abs', Int16Array, iabs(16), Uint16Array],
       ['i32x4.abs', Int32Array, iabs(32), Uint32Array],
       ['f32x4.neg', Float32Array, fneg],
       ['f64x2.neg', Float64Array, fneg],
       ['f32x4.abs', Float32Array, fabs],
       ['f64x2.abs', Float64Array, fabs],
       ['f32x4.sqrt', Float32Array, fsqrt],
       ['f64x2.sqrt', Float64Array, sqrt],
       ['v128.not', Uint8Array, bitnot],
      ])
{
    let [ins, mem, resultmem] = insAndMemUnop(op, memtype, resultmemtype);
    let len = 16/memtype.BYTES_PER_ELEMENT;
    let xs = iota(len);

    function testIt(a) {
        let r = xs.map((i) => rop(a[i]));
        set(mem, len, a);
        ins.exports.run();
        assertSame(get(resultmem, 0, len), r);
    }

    for (let a of memtype.inputs)
        testIt(a);
}

// AnyTrue

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "anytrue_i8x16") (result i32)
      (i8x16.any_true (v128.load (i32.const 16))))
    (func (export "anytrue_i16x8") (result i32)
      (i16x8.any_true (v128.load (i32.const 16))))
    (func (export "anytrue_i32x4") (result i32)
      (i32x4.any_true (v128.load (i32.const 16)))))`);

var mem = new Uint8Array(ins.exports.mem.buffer);
set(mem, 16, iota(16).map((_) => 0));
assertEq(ins.exports.anytrue_i8x16(), 0);
assertEq(ins.exports.anytrue_i16x8(), 0);
assertEq(ins.exports.anytrue_i32x4(), 0);

for ( let dope of [1, 7, 32, 195 ] ) {
    set(mem, 16, iota(16).map((x) => x == 7 ? dope : 0));
    assertEq(ins.exports.anytrue_i8x16(), 1);
    assertEq(ins.exports.anytrue_i16x8(), 1);
    assertEq(ins.exports.anytrue_i32x4(), 1);
}

// AllTrue

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "alltrue_i8x16") (result i32)
      (i8x16.all_true (v128.load (i32.const 16))))
    (func (export "alltrue_i16x8") (result i32)
      (i16x8.all_true (v128.load (i32.const 16))))
    (func (export "alltrue_i32x4") (result i32)
      (i32x4.all_true (v128.load (i32.const 16)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Uint32Array(ins.exports.mem.buffer);

set(mem8, 16, iota(16).map((_) => 0));
assertEq(ins.exports.alltrue_i8x16(), 0);
assertEq(ins.exports.alltrue_i16x8(), 0);
assertEq(ins.exports.alltrue_i32x4(), 0);

set(mem8, 16, iota(16).map((_) => 1));
assertEq(ins.exports.alltrue_i8x16(), 1);

set(mem16, 8, iota(8).map((_) => 1));
assertEq(ins.exports.alltrue_i16x8(), 1);

set(mem32, 4, iota(4).map((_) => 1));
assertEq(ins.exports.alltrue_i32x4(), 1);

for ( let dope of [1, 7, 32, 195 ] ) {
    set(mem8, 16, iota(16).map((x) => x == 7 ? 0 : dope));
    assertEq(ins.exports.alltrue_i8x16(), 0);

    set(mem16, 8, iota(8).map((x) => x == 4 ? 0 : dope));
    assertEq(ins.exports.alltrue_i16x8(), 0);

    set(mem32, 4, iota(4).map((x) => x == 2 ? 0 : dope));
    assertEq(ins.exports.alltrue_i32x4(), 0);
}

// Shifts
//
// lhs is v128 in memory
// rhs is i32 (passed directly)
// result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "shl_i8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (local.get $count)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 4, 8, 16, 32, 64, 128, 129, 130, 132, 136, 144, 160, 192, 255];

set(mem8, 16, as);

for (let [meth,op] of [["shl_i8x16",shl], ["shr_i8x16",shr], ["shr_u8x16",shru]]) {
    for ( let i=0 ; i < 8 ; i++ ) {
        ins.exports[meth](i);
        assertSame(get(mem8, 0, 16), as.map(op(i, 8)))
    }

    ins.exports[meth](1);
    var a = get(mem8, 0, 16);
    ins.exports[meth](9);
    var b = get(mem8, 0, 16);
    assertSame(a, b);
}

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8];

set(mem16, 8, as)
ins.exports.shl_i16x8(2);
assertSame(get(mem16, 0, 8), as.map(shl(2, 16)))

set(mem16, 8, as)
ins.exports.shr_i16x8(1);
assertSame(get(mem16, 0, 8), as.map(shr(1, 16)))

set(mem16, 8, as)
ins.exports.shr_u16x8(1);
assertSame(get(mem16, 0, 8), as.map(shru(1, 16)))

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [5, 6, 7, 8];

set(mem32, 4, as)
ins.exports.shl_i32x4(2);
assertSame(get(mem32, 0, 4), as.map(shl(2, 32)))

set(mem32, 4, as)
ins.exports.shr_i32x4(1);
assertSame(get(mem32, 0, 4), as.map(shr(1, 32)))

set(mem32, 4, as)
ins.exports.shr_u32x4(1);
assertSame(get(mem32, 0, 4), as.map(shru(1, 32)))

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [5, -6];

set(mem64, 2, as)
ins.exports.shl_i64x2(2);
assertSame(get(mem64, 0, 2), as.map(shl(2, 64)))

set(mem64, 2, as)
ins.exports.shr_u64x2(1);
assertSame(get(mem64, 0, 2), as.map(shru(1, 64)))

set(mem64, 2, as)
ins.exports.shr_i64x2(2);
assertSame(get(mem64, 0, 2), as.map(shr(2, 64)))

// Narrow

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "narrow_i16x8_s")
      (v128.store (i32.const 0) (i8x16.narrow_i16x8_s (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i16x8_u")
      (v128.store (i32.const 0) (i8x16.narrow_i16x8_u (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i32x4_s")
      (v128.store (i32.const 0) (i16x8.narrow_i32x4_s (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i32x4_u")
      (v128.store (i32.const 0) (i16x8.narrow_i32x4_u (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem8u = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);

var as = [1, 267, 3987, 14523, 32768, 3, 312, 4876].map((x) => sign_extend(x, 16));
var bs = [2, 312, 4876, 15987, 33777, 1, 267, 3987].map((x) => sign_extend(x, 16));

set(mem16, 8, as);
set(mem16, 16, bs);

ins.exports.narrow_i16x8_s();
var cs = as.concat(...bs).map((x) => signed_saturate(x, 8));
assertSame(get(mem8, 0, 16), cs);

ins.exports.narrow_i16x8_u();
var cs = as.concat(...bs).map((x) => unsigned_saturate(x, 8));
assertSame(get(mem8u, 0, 16), cs);

var xs = [1, 3987, 14523, 32768].map((x) => x << 16).map((x) => sign_extend(x, 32));
var ys = [2, 4876, 15987, 33777].map((x) => x << 16).map((x) => sign_extend(x, 32));

set(mem32, 4, xs);
set(mem32, 8, ys);

ins.exports.narrow_i32x4_s();
var cs = xs.concat(...ys).map((x) => signed_saturate(x, 16));
assertSame(get(mem16, 0, 8), cs);

ins.exports.narrow_i32x4_u();
var cs = xs.concat(...ys).map((x) => unsigned_saturate(x, 16));
assertSame(get(mem16u, 0, 8), cs);

// Widen low/high

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "widen_low_i8x16_s")
      (v128.store (i32.const 0) (i16x8.widen_low_i8x16_s (v128.load (i32.const 16)))))
    (func (export "widen_high_i8x16_s")
      (v128.store (i32.const 0) (i16x8.widen_high_i8x16_s (v128.load (i32.const 16)))))
    (func (export "widen_low_i8x16_u")
      (v128.store (i32.const 0) (i16x8.widen_low_i8x16_u (v128.load (i32.const 16)))))
    (func (export "widen_high_i8x16_u")
      (v128.store (i32.const 0) (i16x8.widen_high_i8x16_u (v128.load (i32.const 16)))))
    (func (export "widen_low_i16x8_s")
      (v128.store (i32.const 0) (i32x4.widen_low_i16x8_s (v128.load (i32.const 16)))))
    (func (export "widen_high_i16x8_s")
      (v128.store (i32.const 0) (i32x4.widen_high_i16x8_s (v128.load (i32.const 16)))))
    (func (export "widen_low_i16x8_u")
      (v128.store (i32.const 0) (i32x4.widen_low_i16x8_u (v128.load (i32.const 16)))))
    (func (export "widen_high_i16x8_u")
      (v128.store (i32.const 0) (i32x4.widen_high_i16x8_u (v128.load (i32.const 16))))))`);

var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem8 =  new Int8Array(ins.exports.mem.buffer);
var as = [0, 1, 192, 3, 205, 5, 6, 133, 8, 9, 129, 11, 201, 13, 14, 255];

set(mem8, 16, as);

ins.exports.widen_low_i8x16_s();
assertSame(get(mem16, 0, 8), iota(8).map((n) => sign_extend(as[n], 8)));

ins.exports.widen_high_i8x16_s();
assertSame(get(mem16, 0, 8), iota(8).map((n) => sign_extend(as[n+8], 8)));

ins.exports.widen_low_i8x16_u();
assertSame(get(mem16u, 0, 8), iota(8).map((n) => zero_extend(as[n], 8)));

ins.exports.widen_high_i8x16_u();
assertSame(get(mem16u, 0, 8), iota(8).map((n) => zero_extend(as[n+8], 8)));

var mem32 = new Int16Array(ins.exports.mem.buffer);
var mem32u = new Uint16Array(ins.exports.mem.buffer);

var as = [0, 1, 192, 3, 205, 5, 6, 133].map((x) => x << 16);

set(mem16, 8, as);

ins.exports.widen_low_i16x8_s();
assertSame(get(mem32, 0, 4), iota(4).map((n) => sign_extend(as[n], 16)));

ins.exports.widen_high_i16x8_s();
assertSame(get(mem32, 0, 4), iota(4).map((n) => sign_extend(as[n+4], 16)));

ins.exports.widen_low_i16x8_u();
assertSame(get(mem32u, 0, 4), iota(4).map((n) => zero_extend(as[n], 16)));

ins.exports.widen_high_i16x8_u();
assertSame(get(mem32u, 0, 4), iota(4).map((n) => zero_extend(as[n+4], 16)));


// Extract lane
//
// operand is v128 in memory
// lane index is immediate so we're testing something randomish but not zero
// result is scalar (returned directly)

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "extract_i8x16_9") (result i32)
      (i8x16.extract_lane_s 9 (v128.load (i32.const 16))))
    (func (export "extract_u8x16_6") (result i32)
      (i8x16.extract_lane_u 6 (v128.load (i32.const 16))))
    (func (export "extract_i16x8_5") (result i32)
      (i16x8.extract_lane_s 5 (v128.load (i32.const 16))))
    (func (export "extract_u16x8_3") (result i32)
      (i16x8.extract_lane_u 3 (v128.load (i32.const 16))))
    (func (export "extract_i32x4_2") (result i32)
      (i32x4.extract_lane 2 (v128.load (i32.const 16))))
    (func (export "extract_i64x2_1") (result i64)
      (i64x2.extract_lane 1 (v128.load (i32.const 16))))
    (func (export "extract_f32x4_2") (result f32)
      (f32x4.extract_lane 2 (v128.load (i32.const 16))))
    (func (export "extract_f64x2_1") (result f64)
      (f64x2.extract_lane 1 (v128.load (i32.const 16)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
var bs = as.map((x) => -x);

set(mem8, 16, as)
assertEq(ins.exports.extract_i8x16_9(), as[9]);

set(mem8, 16, bs)
assertEq(ins.exports.extract_u8x16_6(), 256 - as[6]);

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8];
var bs = as.map((x) => -x);

set(mem16, 8, as)
assertEq(ins.exports.extract_i16x8_5(), as[5]);

set(mem16, 8, bs)
assertEq(ins.exports.extract_u16x8_3(), 65536 - as[3]);

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4];

set(mem32, 4, as)
assertEq(ins.exports.extract_i32x4_2(), as[2]);

var mem32 = new Float32Array(ins.exports.mem.buffer);
var as = [1.5, 2.5, 3.5, 4.5];

set(mem32, 4, as)
assertEq(ins.exports.extract_f32x4_2(), as[2]);

var mem64 = new Float64Array(ins.exports.mem.buffer);
var as = [1.5, 2.5];

set(mem64, 2, as)
assertEq(ins.exports.extract_f64x2_1(), as[1]);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [12345, 67890];

set(mem64, 2, as)
assertSame(ins.exports.extract_i64x2_1(), as[1]);

// Replace lane
//
// operand 1 is v128 in memory
// operand 2 is immediate scalar
// lane index is immediate so we're testing something randomish but not zero
// result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "replace_i8x16_9") (param $value i32)
      (v128.store (i32.const 0)
        (i8x16.replace_lane 9 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i16x8_5") (param $value i32)
      (v128.store (i32.const 0)
        (i16x8.replace_lane 5 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i32x4_3") (param $value i32)
      (v128.store (i32.const 0)
        (i32x4.replace_lane 3 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i64x2_1") (param $value i64)
      (v128.store (i32.const 0)
        (i64x2.replace_lane 1 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f32x4_3") (param $value f32)
      (v128.store (i32.const 0)
        (f32x4.replace_lane 3 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f64x2_0") (param $value f64)
      (v128.store (i32.const 0)
        (f64x2.replace_lane 0 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f64x2_1") (param $value f64)
      (v128.store (i32.const 0)
        (f64x2.replace_lane 1 (v128.load (i32.const 16)) (local.get $value)))))`);


var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

set(mem8, 16, as)
ins.exports.replace_i8x16_9(42);
assertSame(get(mem8, 0, 16), upd(as, 9, 42));

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8];

set(mem16, 8, as)
ins.exports.replace_i16x8_5(42);
assertSame(get(mem16, 0, 8), upd(as, 5, 42));

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4];

set(mem32, 4, as)
ins.exports.replace_i32x4_3(42);
assertSame(get(mem32, 0, 4), upd(as, 3, 42));

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [1, 2];

set(mem64, 2, as)
ins.exports.replace_i64x2_1(42n);
assertSame(get(mem64, 0, 2), upd(as, 1, 42));

var mem32 = new Float32Array(ins.exports.mem.buffer);
var as = [1.5, 2.5, 3.5, 4.5];

set(mem32, 4, as)
ins.exports.replace_f32x4_3(42.5);
assertSame(get(mem32, 0, 4), upd(as, 3, 42.5));

var mem64 = new Float64Array(ins.exports.mem.buffer);
var as = [1.5, 2.5];

set(mem64, 2, as)
ins.exports.replace_f64x2_0(42.5);
assertSame(get(mem64, 0, 2), upd(as, 0, 42.5));

set(mem64, 2, as)
ins.exports.replace_f64x2_1(42.5);
assertSame(get(mem64, 0, 2), upd(as, 1, 42.5));

// Load and splat
//
// Operand is memory address of scalar
// Result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "load_splat_v8x16") (param $addr i32)
      (v128.store (i32.const 0) (v8x16.load_splat (local.get $addr))))
    (func (export "load_splat_v16x8") (param $addr i32)
      (v128.store (i32.const 0) (v16x8.load_splat (local.get $addr))))
    (func (export "load_splat_v32x4") (param $addr i32)
      (v128.store (i32.const 0) (v32x4.load_splat (local.get $addr))))
    (func (export "load_splat_v64x2") (param $addr i32)
      (v128.store (i32.const 0) (v64x2.load_splat (local.get $addr)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
mem8[37] = 42;
ins.exports.load_splat_v8x16(37);
assertSame(get(mem8, 0, 16), [42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42]);

var mem16 = new Uint16Array(ins.exports.mem.buffer);
mem16[37] = 69;
ins.exports.load_splat_v16x8(37*2);
assertSame(get(mem16, 0, 8), [69, 69, 69, 69, 69, 69, 69, 69]);

var mem32 = new Int32Array(ins.exports.mem.buffer);
mem32[37] = 83;
ins.exports.load_splat_v32x4(37*4);
assertSame(get(mem32, 0, 4), [83, 83, 83, 83]);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
mem64[37] = 83n;
ins.exports.load_splat_v64x2(37*8);
assertSame(get(mem64, 0, 2), [83, 83]);

// Load and extend
//
// Operand is memory address of 64-bit scalar representing 8, 4, or 2 values
// Result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "load8x8_s") (param $addr i32)
      (v128.store (i32.const 0) (i16x8.load8x8_s (local.get $addr))))
    (func (export "load8x8_u") (param $addr i32)
      (v128.store (i32.const 0) (i16x8.load8x8_u (local.get $addr))))
    (func (export "load16x4_s") (param $addr i32)
      (v128.store (i32.const 0) (i32x4.load16x4_s (local.get $addr))))
    (func (export "load16x4_u") (param $addr i32)
      (v128.store (i32.const 0) (i32x4.load16x4_u (local.get $addr))))
    (func (export "load32x2_s") (param $addr i32)
      (v128.store (i32.const 0) (i64x2.load32x2_s (local.get $addr))))
    (func (export "load32x2_u") (param $addr i32)
      (v128.store (i32.const 0) (i64x2.load32x2_u (local.get $addr)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem16s = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32u = new Uint32Array(ins.exports.mem.buffer);
var mem64s = new BigInt64Array(ins.exports.mem.buffer);
var mem64u = new BigUint64Array(ins.exports.mem.buffer);
var xs = [42, 129, 2, 212, 44, 27, 12, 199];
set(mem8, 48, xs);

ins.exports.load8x8_s(48);
assertSame(get(mem16s, 0, 8), xs.map((x) => sign_extend(x, 8)));

ins.exports.load8x8_u(48);
assertSame(get(mem16u, 0, 8), xs.map((x) => zero_extend(x, 8)));

var xs = [(42 << 8) | 129, (212 << 8) | 2, (44 << 8) | 27, (199 << 8) | 12];
set(mem16u, 24, xs);

ins.exports.load16x4_s(48);
assertSame(get(mem32s, 0, 4), xs.map((x) => sign_extend(x, 16)));

ins.exports.load16x4_u(48);
assertSame(get(mem32u, 0, 4), xs.map((x) => zero_extend(x, 16)));

var xs = [5, -8];
set(mem32u, 12, xs);

ins.exports.load32x2_s(48);
assertSame(get(mem64s, 0, 2), xs.map((x) => sign_extend(x, 32)));

ins.exports.load32x2_u(48);
assertSame(get(mem64s, 0, 2), xs.map((x) => zero_extend(x, 32)));

// Vector select
//
// Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "bitselect_v128")
      (v128.store (i32.const 0)
        (v128.bitselect (v128.load (i32.const 16))
                        (v128.load (i32.const 32))
                        (v128.load (i32.const 48))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
set(mem8, 16, iota(16).map((_) => 0xAA));
set(mem8, 32, iota(16).map((_) => 0x55));

set(mem8, 48, iota(16).map((_) => 0x99));
ins.exports.bitselect_v128();
assertSame(get(mem8, 0, 16), iota(16).map((_) => 0xCC));

set(mem8, 48, iota(16).map((_) => 0x77));
ins.exports.bitselect_v128();
assertSame(get(mem8, 0, 16), iota(16).map((_) => 0x22));

// Vector shuffle
//
// Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    ;; the result interleaves the low eight bytes of the inputs
    (func (export "shuffle1")
      (v128.store (i32.const 0)
        (v8x16.shuffle 0 16 1 17 2 18 3 19 4 20 5 21 6 22 7 23
           (v128.load (i32.const 16))
           (v128.load (i32.const 32)))))
    ;; ditto the high eight bytes
    (func (export "shuffle2")
      (v128.store (i32.const 0)
        (v8x16.shuffle 8 24 9 25 10 26 11 27 12 28 13 29 14 30 15 31
           (v128.load (i32.const 16))
           (v128.load (i32.const 32))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var xs = iota(16).map((n) => 0xA0 + n);
var ys = iota(16).map((n) => 0x50 + n);
set(mem8, 16, xs);
set(mem8, 32, ys);

ins.exports.shuffle1();
assertSame(get(mem8, 0, 16), iota(16).map((x) => ((x & 1) ? ys : xs)[x >>> 1]))

ins.exports.shuffle2();
assertSame(get(mem8, 0, 16), iota(32).map((x) => ((x & 1) ? ys : xs)[x >>> 1]).slice(16));

// Vector swizzle (variable permute).
//
// Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "swizzle")
      (v128.store (i32.const 0)
        (v8x16.swizzle (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);

var xs = [100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115];
set(mem8, 16, xs);

set(mem8, 32, [1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14]);
ins.exports.swizzle();
assertSame(get(mem8, 0, 16), [101,100,103,102,105,104,107,106,109,108,111,110,113,112,115,114]);

set(mem8, 32, [9,8,11,10,13,12,16,14,1,0,3,2,5,192,7,6]);
ins.exports.swizzle();
assertSame(get(mem8, 0, 16), [109,108,111,110,113,112,0,114,101,100,103,102,105,0,107,106]);

// Convert integer to floating point

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "convert_s")
      (v128.store (i32.const 0)
        (f32x4.convert_i32x4_s (v128.load (i32.const 16)))))
    (func (export "convert_u")
      (v128.store (i32.const 0)
        (f32x4.convert_i32x4_u (v128.load (i32.const 16))))))`);

var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32f = new Float32Array(ins.exports.mem.buffer);
var xs = [1, -9, 77987, -34512];

set(mem32s, 4, xs);
ins.exports.convert_s();
assertSame(get(mem32f, 0, 4), xs);

var mem32u = new Uint32Array(ins.exports.mem.buffer);
var ys = xs.map((x) => x>>>0);

set(mem32u, 4, ys);
ins.exports.convert_u();
assertSame(get(mem32f, 0, 4), ys.map(Math.fround));

// Convert floating point to integer with saturating truncation

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "trunc_sat_s")
      (v128.store (i32.const 0)
        (i32x4.trunc_sat_f32x4_s (v128.load (i32.const 16)))))
    (func (export "trunc_sat_u")
      (v128.store (i32.const 0)
        (i32x4.trunc_sat_f32x4_u (v128.load (i32.const 16))))))`);

var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32u = new Uint32Array(ins.exports.mem.buffer);
var mem32f = new Float32Array(ins.exports.mem.buffer);
var xs = [1.5, -9.5, 7.5e12, -8e13];

set(mem32f, 4, xs);
ins.exports.trunc_sat_s();
assertSame(get(mem32s, 0, 4), [1, -9, 0x7FFFFFFF, -0x80000000]);

var xs = [1.5, -9.5, 7.5e12, 812];
set(mem32f, 4, xs);
ins.exports.trunc_sat_u();
assertSame(get(mem32u, 0, 4), [1, 0, 0xFFFFFFFF, 812]);

var xs = [0, -0, 0x80860000, 0x100000000];
set(mem32f, 4, xs);
ins.exports.trunc_sat_u();
assertSame(get(mem32u, 0, 4), [0, 0, 0x80860000, 0xFFFFFFFF]);

// Loops and blocks.  This should at least test "sync" in the baseline compiler.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $f (param $count i32) (param $v v128) (result v128)
      (local $tmp v128)
      (block $B1
        (loop $L1
          (br_if $B1 (i32.eqz (local.get $count)))
          (local.set $tmp (i32x4.add (local.get $tmp) (local.get $v)))
          (local.set $count (i32.sub (local.get $count) (i32.const 1)))
          (br $L1)))
      (local.get $tmp))
    (func (export "run") (param $count i32)
      (v128.store (i32.const 0)
        (call $f (local.get $count) (v128.load (i32.const 16))))))`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
set(mem32, 4, [1,2,3,4]);
ins.exports.run(7);
assertSame(get(mem32, 0, 4), [7,14,21,28]);

// Lots of parameters, this should trigger stack parameter passing
//
// 10 parameters in memory, we load them and pass them and operate on them.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $f (param $v0 v128) (param $v1 v128) (param $v2 v128) (param $v3 v128) (param $v4 v128)
             (param $v5 v128) (param $v6 v128) (param $v7 v128) (param $v8 v128) (param $v9 v128)
             (result v128)
      (i32x4.add (local.get $v0)
        (i32x4.add (local.get $v1)
          (i32x4.add (local.get $v2)
            (i32x4.add (local.get $v3)
              (i32x4.add (local.get $v4)
                (i32x4.add (local.get $v5)
                  (i32x4.add (local.get $v6)
                    (i32x4.add (local.get $v7)
                      (i32x4.add (local.get $v8) (local.get $v9)))))))))))
    (func (export "run")
      (v128.store (i32.const 0)
        (call $f (v128.load (i32.const ${16*1}))
                 (v128.load (i32.const ${16*2}))
                 (v128.load (i32.const ${16*3}))
                 (v128.load (i32.const ${16*4}))
                 (v128.load (i32.const ${16*5}))
                 (v128.load (i32.const ${16*6}))
                 (v128.load (i32.const ${16*7}))
                 (v128.load (i32.const ${16*8}))
                 (v128.load (i32.const ${16*9}))
                 (v128.load (i32.const ${16*10}))))))`);


var mem32 = new Int32Array(ins.exports.mem.buffer);
var sum = [0, 0, 0, 0];
for ( let i=1; i <= 10; i++ ) {
    let v = [1,2,3,4].map((x) => x*i);
    set(mem32, 4*i, v);
    for ( let j=0; j < 4; j++ )
        sum[j] += v[j];
}

ins.exports.run();

assertSame(get(mem32, 0, 4), sum);

// Globals, basic operations.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)

    (global $g (mut v128) (v128.const i32x4 0 0 0 0))

    (func (export "put") (param $val i32)
      (global.set $g (i32x4.splat (local.get $val))))

    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`);

var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.put(37);
ins.exports.get(0);
assertSame(get(mem, 0, 4), [37, 37, 37, 37]);

// Imports and exports that pass and return v128

var insworker = wasmEvalText(`
  (module
    (func (export "worker") (param v128) (result v128)
      (i8x16.add (local.get 0) (v128.const i8x16 ${iota(16).join(' ')}))))`);

var insrun = wasmEvalText(`
  (module
    (import "" "worker" (func $worker (param v128) (result v128)))
    (memory (export "mem") 1 1)
    (func (export "run") (param $srcloc i32) (param $destloc i32)
      (v128.store (local.get $destloc)
        (call $worker (v128.load (local.get $srcloc))))))`,
                          {"":insworker.exports});

var mem = new Uint8Array(insrun.exports.mem.buffer);
var xs = iota(16).map((x) => x+5);
set(mem, 0, xs);
insrun.exports.run(0, 16);
assertSame(get(mem, 16, 16), xs.map((x,i) => x+i))

// Imports and exports of v128 globals

var insexporter = wasmEvalText(`
  (module
    (global (export "myglobal") (mut v128) (v128.const i8x16 ${iota(16).join(' ')})))`);

var insimporter = wasmEvalText(`
  (module
    (import "m" "myglobal" (global $g (mut v128)))
    (memory (export "mem") 1 1)
    (func (export "run") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                               {m:insexporter.exports});

var mem = new Uint8Array(insimporter.exports.mem.buffer);
insimporter.exports.run(16);
assertSame(get(mem, 16, 16), iota(16));

// Multi-value cases

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $mvreturn (result v128 v128 v128)
      (v128.load (i32.const 16))
      (v128.load (i32.const 0))
      (v128.load (i32.const 32)))
    (func (export "runreturn")
      i32.const 48
      (call $mvreturn)
      i32x4.sub ;; [-20, -20, -20, -20]
      i32x4.sub ;; [31, 32, 33, 34]
      v128.store)
    (func (export "runif") (param $cond i32)
      i32.const 48
      (v128.load (i32.const 0))
      (v128.load (i32.const 16))
      (if (param v128) (param v128) (result v128 v128)
          (local.get $cond)
          (then i32x4.add
                (v128.load (i32.const 32)))
          (else i32x4.sub
                (v128.load (i32.const 0))))
      i32x4.add
      v128.store))`);

var mem = new Int32Array(ins.exports.mem.buffer);
set(mem, 0, [1, 2, 3, 4]);
set(mem, 4, [11, 12, 13, 14]);
set(mem, 8, [21, 22, 23, 24]);

// Multi-value returns

ins.exports.runreturn();
assertSame(get(mem, 12, 4), [31, 32, 33, 34]);

// Multi-parameters to and multi-returns from "if"

// This should be vector@0 + vector@16 + vector@32
ins.exports.runif(1);
assertSame(get(mem, 12, 4),
           [33, 36, 39, 42]);

// This should be vector@0 - vector@16 + vector@0
ins.exports.runif(0);
assertSame(get(mem, 12, 4),
           [-9, -8, -7, -6]);
