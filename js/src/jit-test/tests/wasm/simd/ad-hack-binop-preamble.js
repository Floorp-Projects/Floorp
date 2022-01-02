// |jit-test| skip-if: true

// Common code to test simple binary operators.  See runSimpleBinopTest below.

function expandConstantBinopInputs(op, memtype, inputs) {
    let s = '';
    let ident = 0;
    for ( let [a, b] of inputs ) {
        let constlhs = `${memtype.layoutName} ${a.map(jsValueToWasmName).join(' ')}`;
        let constrhs = `${memtype.layoutName} ${b.map(jsValueToWasmName).join(' ')}`;
        s += `
    ;; lhs is constant, rhs is variable
    (func (export "run_constlhs${ident}")
      (v128.store (i32.const 0)
        (call $doit_constlhs${ident} (v128.const ${constrhs}))))
    (func $doit_constlhs${ident} (param $b v128) (result v128)
      (${op} (v128.const ${constlhs}) (local.get $b)))

    ;; rhs is constant, lhs is variable
    (func (export "run_constrhs${ident}")
      (v128.store (i32.const 0)
        (call $doit_constrhs${ident} (v128.const ${constlhs}))))
    (func $doit_constrhs${ident} (param $a v128) (result v128)
      (${op} (local.get $a) (v128.const ${constrhs})))

    ;; both operands are constant
    (func (export "run_constboth${ident}")
      (v128.store (i32.const 0)
        (call $doit_constboth${ident})))
    (func $doit_constboth${ident} (result v128)
      (${op} (v128.const ${constlhs}) (v128.const ${constrhs})))`
        ident++;
    }
    return s;
}

function insAndMemBinop(op, memtype, resultmemtype, inputs) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)

    ;; both arguments are variable
    (func (export "run")
      (v128.store (i32.const 0)
        (call $doit (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func $doit (param $a v128) (param $b v128) (result v128)
      (${op} (local.get $a) (local.get $b)))

    ${expandConstantBinopInputs(op, memtype, inputs)})`);
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
function fmin(x, y) {
    if (x == y) return x;
    if (x < y) return x;
    if (y < x) return y;
    if (isNaN(x)) return x;
    return y;
}
function fmax(x, y) {
    if (x == y) return x;
    if (x > y) return x;
    if (y > x) return y;
    if (isNaN(x)) return x;
    return y;
}
function dadd(x, y) { return x+y }
function dsub(x, y) { return x-y }
function dmul(x, y) { return x*y }
function ddiv(x, y) { return x/y }
var dmax = fmax;
var dmin = fmin;

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

function pmin(x, y) { return y < x ? y : x }
function pmax(x, y) { return x < y ? y : x }

assertEq(max_s(8)(1, 2), 2);
assertEq(max_s(8)(1, 128), 1);
assertEq(min_s(8)(1, 2), 1);
assertEq(min_s(8)(1, 128), -128);
assertEq(max_u(8)(1, 2), 2);
assertEq(max_u(8)(1, 128), 128);
assertEq(min_u(8)(1, 2), 1);
assertEq(min_u(8)(1, 128), 1);

var binopTests =
    [['i8x16.add', Int8Array, add(8)],
     ['i16x8.add', Int16Array, add(16)],
     ['i32x4.add', Int32Array, add(32)],
     ['i64x2.add', BigInt64Array, add64],
     ['i8x16.sub', Int8Array, sub(8)],
     ['i16x8.sub', Int16Array, sub(16)],
     ['i32x4.sub', Int32Array, sub(32)],
     ['i64x2.sub', BigInt64Array, sub64],
     ['i8x16.add_sat_s', Int8Array, add_sat_s(8)],
     ['i8x16.add_sat_u', Uint8Array, add_sat_u(8)],
     ['i16x8.add_sat_s', Int16Array, add_sat_s(16)],
     ['i16x8.add_sat_u', Uint16Array, add_sat_u(16)],
     ['i8x16.sub_sat_s', Int8Array, sub_sat_s(8)],
     ['i8x16.sub_sat_u', Uint8Array, sub_sat_u(8)],
     ['i16x8.sub_sat_s', Int16Array, sub_sat_s(16)],
     ['i16x8.sub_sat_u', Uint16Array, sub_sat_u(16)],
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
     ['f32x4.min', Float32Array, fmin],
     ['f32x4.max', Float32Array, fmax],
     ['f64x2.add', Float64Array, dadd],
     ['f64x2.sub', Float64Array, dsub],
     ['f64x2.mul', Float64Array, dmul],
     ['f64x2.div', Float64Array, ddiv],
     ['f64x2.min', Float64Array, dmin],
     ['f64x2.max', Float64Array, dmax],
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
     ['f32x4.pmin', Float32Array, pmin],
     ['f32x4.pmax', Float32Array, pmax],
     ['f64x2.pmin', Float64Array, pmin],
     ['f64x2.pmax', Float64Array, pmax]]

// Run v128 x v128 -> v128 tests.  Inputs are taken from the common input sets,
// placed in memory, the test is run, and the result is extracted and checked.
//
// Runs tests with both operands as variables, either as constant, or both as
// constant.  Also checks NaN behavior when appropriate.
//
// All runners that call this should use the same value for `ofParts` and should
// pass different values for `part`, up to `ofParts` - 1.

function runSimpleBinopTest(part, ofParts) {
    let partSize = Math.ceil(binopTests.length / ofParts);
    let start = part * partSize;
    let end = Math.min((part + 1) * partSize, binopTests.length);
    for ( let [op, memtype, rop, resultmemtype] of binopTests.slice(start, end) ) {
        let inputs = cross(memtype.inputs);
        let len = 16/memtype.BYTES_PER_ELEMENT;
        let xs = iota(len);
        let zero = xs.map(_ => 0);
        let [ins, mem, resultmem] = insAndMemBinop(op, memtype, resultmemtype, inputs);
        let bitsForF32 = memtype == Float32Array ? new Uint32Array(mem.buffer) : null;
        let bitsForF64 = memtype == Float64Array ? new BigInt64Array(mem.buffer) : null;

        function testIt(a,b,r) {
            set(mem, len, a);
            set(mem, len*2, b);
            ins.exports.run();
            assertSame(get(resultmem, 0, len), r);

            // Test signalling NaN superficially by replacing QNaN inputs with SNaN
            if (bitsForF32 != null && (a.some(isNaN) || b.some(isNaN))) {
                a.forEach((x, i) => { if (isNaN(x)) { bitsForF32[len+i] = 0x7FA0_0000; } });
                b.forEach((x, i) => { if (isNaN(x)) { bitsForF32[(len*2)+i] = 0x7FA0_0000; } });
                ins.exports.run();
                assertSame(get(resultmem, 0, len), r);
            }
            if (bitsForF64 != null && (a.some(isNaN) || b.some(isNaN))) {
                a.forEach((x, i) => { if (isNaN(x)) { bitsForF64[len+i] = 0x7FF4_0000_0000_0000n; } });
                b.forEach((x, i) => { if (isNaN(x)) { bitsForF64[(len*2)+i] = 0x7FF4_0000_0000_0000n; } });
                ins.exports.run();
                assertSame(get(resultmem, 0, len), r);
            }
        }

        function testConstIt(i,r) {
            set(resultmem, 0, zero);
            ins.exports["run_constlhs" + i]();
            assertSame(get(resultmem, 0, len), r);

            set(resultmem, 0, zero);
            ins.exports["run_constrhs" + i]();
            assertSame(get(resultmem, 0, len), r);

            set(resultmem, 0, zero);
            ins.exports["run_constboth" + i]();
            assertSame(get(resultmem, 0, len), r);
        }

        let i = 0;
        for (let [a,b] of inputs) {
            let r = xs.map((i) => rop(a[i], b[i]));
            testIt(a,b,r);
            testConstIt(i,r);
            i++;
        }
    }
}
