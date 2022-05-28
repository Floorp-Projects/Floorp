// |jit-test| skip-if: !wasmSimdEnabled()

// Do not include this in the preamble, it must be loaded after lib/wasm.js
load(scriptdir + "ad-hack-preamble.js")

// Simple unary operators.  Place parameter in memory at offset 16,
// read the result at offset 0.

function expandConstantUnopInputs(op, memtype, inputs) {
    let s = '';
    let ident = 0;
    for ( let a of inputs ) {
        let constval = `${memtype.layoutName} ${a.map(jsValueToWasmName).join(' ')}`;
        s += `
    (func (export "run_const${ident}")
      (v128.store (i32.const 0)
        (${op} (v128.const ${constval}))))
`;
        ident++;
    }
    return s;
}

function insAndMemUnop(op, memtype, resultmemtype, inputs) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)

    (func (export "run")
      (v128.store (i32.const 0)
        (call $doit (v128.load (i32.const 16)))))

    (func $doit (param $a v128) (result v128)
      (${op} (local.get $a)))

    ${expandConstantUnopInputs(op, memtype, inputs)})`);
    var mem = new memtype(ins.exports.mem.buffer);
    var resultmem = !resultmemtype || memtype == resultmemtype ? mem : new resultmemtype(ins.exports.mem.buffer);
    return [ins, mem, resultmem];
}

function ineg(bits) { return (a) => sign_extend(!a ? a : -a,bits) }
function iabs(bits) { return (a) => zero_extend(a < 0 ? -a : a, bits) }
function fneg(a) { return -a }
function fabs(a) { return Math.abs(a) }
function fsqrt(a) { return Math.fround(Math.sqrt(Math.fround(a))) }
function dsqrt(a) { return Math.sqrt(a) }
function bitnot(a) { return (~a) & 255 }
function ffloor(x) { return Math.fround(Math.floor(x)) }
function fceil(x) { return Math.fround(Math.ceil(x)) }
function ftrunc(x) { return Math.fround(Math.sign(x)*Math.floor(Math.abs(x))) }
function fnearest(x) { return Math.fround(Math.round(x)) }
function dfloor(x) { return Math.floor(x) }
function dceil(x) { return Math.ceil(x) }
function dtrunc(x) { return Math.sign(x)*Math.floor(Math.abs(x)) }
function dnearest(x) { return Math.round(x) }

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
       ['f64x2.sqrt', Float64Array, dsqrt],
       ['f32x4.ceil', Float32Array, fceil],
       ['f32x4.floor', Float32Array, ffloor],
       ['f32x4.trunc', Float32Array, ftrunc],
       ['f32x4.nearest', Float32Array, fnearest],
       ['f64x2.ceil', Float64Array, dceil],
       ['f64x2.floor', Float64Array, dfloor],
       ['f64x2.trunc', Float64Array, dtrunc],
       ['f64x2.nearest', Float64Array, dnearest],
       ['v128.not', Uint8Array, bitnot],
      ])
{
    let [ins, mem, resultmem] = insAndMemUnop(op, memtype, resultmemtype, memtype.inputs);
    let len = 16/memtype.BYTES_PER_ELEMENT;
    let xs = iota(len);
    let zero = xs.map(_ => 0);
    let bitsForF32 = memtype == Float32Array ? new Uint32Array(mem.buffer) : null;
    let bitsForF64 = memtype == Float64Array ? new BigInt64Array(mem.buffer) : null;

    function testIt(a, r) {
        set(mem, len, a);
        ins.exports.run();
        assertSame(get(resultmem, 0, len), r);

        // Test signalling NaN superficially by replacing QNaN inputs with SNaN
        if (bitsForF32 != null && a.some(isNaN)) {
            a.forEach((x, i) => { if (isNaN(x)) { bitsForF32[len+i] = 0x7FA0_0000; } });
            ins.exports.run();
            assertSame(get(resultmem, 0, len), r);
        }
        if (bitsForF64 != null && a.some(isNaN)) {
            a.forEach((x, i) => { if (isNaN(x)) { bitsForF64[len+i] = 0x7FF4_0000_0000_0000n; } });
            ins.exports.run();
            assertSame(get(resultmem, 0, len), r);
        }
    }

    function testConstIt(i,r) {
        set(resultmem, 0, zero);
        ins.exports["run_const" + i]();
        assertSame(get(resultmem, 0, len), r);
    }

    let i = 0;
    for (let a of memtype.inputs) {
        let r = xs.map((i) => rop(a[i]));
        testIt(a, r);
        testConstIt(i, r);
        i++;
    }
}

