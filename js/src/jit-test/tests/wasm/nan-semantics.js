load(libdir + "wasm.js");

var f64 = new Float64Array(2);
var f32 = new Float32Array(f64.buffer);
var u8 = new Uint8Array(f64.buffer);

function assertSameBitPattern(from, to, offset) {
    for (let i = from; i < to; i++)
        assertEq(u8[i], u8[i + offset], 'non equality in assertSameBitPattern');
}

// Check that custom NaN can't escape to normal JS, in non-testing mode.
f32[0] = NaN;
f32[0] = f32[0]; // Force canonicalization.

f32[1] = wasmEvalText('(module (func (result f32) (f32.const nan:0x123456)) (export "" 0))')();
assertSameBitPattern(0, 4, 4);

var checkBitPatterns = {
    "": {
        float32(x) {
            f32[1] = x;
            assertSameBitPattern(0, 4, 4);
        },
        float64(x) {
            f64[1] = x;
            assertSameBitPattern(0, 8, 8);
        }
    }
}

wasmEvalText('(module (import "" "float32" (param f32)) (func (call_import 0 (f32.const nan:0x123456))) (export "" 0))', checkBitPatterns)();

f64[0] = NaN;
f64[0] = f64[0]; // Force canonicalization.
f64[1] = wasmEvalText('(module (func (result f64) (f64.const nan:0x123456)) (export "" 0))')();
assertSameBitPattern(0, 8, 8);

wasmEvalText('(module (import "" "float64" (param f64)) (func (call_import 0 (f64.const nan:0x123456))) (export "" 0))', checkBitPatterns)();

// Enable test mode.
setJitCompilerOption('wasm.test-mode', 1);

// SANITY CHECKS

// There are two kinds of NaNs: signaling and quiet. Usually, the first bit of
// the payload is used to indicate whether it is quiet (1 for quiet, 0 for
// signaling). Most operations have to transform a signaling NaN into a quiet
// NaN, which prevents some optimizations in WebAssembly.

// A float32 has 32 bits, 23 bits of them being reserved for the mantissa
// (= NaN payload).
var f32_nan_base = 0x7f800000;

var f32_snan_code = '(f32.const nan:0x200000)';
var f32_snan = wasmEvalText(`(module (func (result f32) ${f32_snan_code}) (export "" 0))`)();
assertEqNaN(f32_snan, { nan_low: f32_nan_base | 0x200000 });

var f32_qnan_code = '(f32.const nan:0x600000)';
var f32_qnan = wasmEvalText(`(module (func (result f32) ${f32_qnan_code}) (export "" 0))`)();
assertEqNaN(f32_qnan, { nan_low: f32_nan_base | 0x600000 });

// A float64 has 64 bits, 1 for the sign, 11 for the exponent, the rest for the
// mantissa (payload).
var f64_nan_base_high = 0x7ff00000;

var f64_snan_code = '(f64.const nan:0x4000000000000)';
var f64_snan = wasmEvalText(`(module (func (result f64) ${f64_snan_code}) (export "" 0))`)();
assertEqNaN(f64_snan, { nan_low: 0x0, nan_high: f64_nan_base_high | 0x40000 });

var f64_qnan_code = '(f64.const nan:0xc000000000000)';
var f64_qnan = wasmEvalText(`(module (func (result f64) ${f64_qnan_code}) (export "" 0))`)();
assertEqNaN(f64_qnan, { nan_low: 0x0, nan_high: f64_nan_base_high | 0xc0000 });

// Actual tests.

// An example where a signaling nan gets transformed into a quiet nan:
// snan + 0.0 = qnan
var nan = wasmEvalText(`(module (func (result f32) (f32.add ${f32_snan_code} (f32.const 0))) (export "" 0))`)();
assertEqNaN(nan, f32_qnan);

// Globals.
var m = evalText(`(module
    (import "globals" "x" (global f32 immutable))
    (func (result f32) (get_global 0))
    (export "global" global 0)
    (export "test" 0))
`, { globals: { x: f32_snan } }).exports;

assertEqNaN(m.test(), f32_snan);
assertEqNaN(m.global, f32_snan);

var m = evalText(`(module
    (import "globals" "x" (global f64 immutable))
    (func (result f64) (get_global 0))
    (export "global" global 0)
    (export "test" 0))
`, { globals: { x: f64_snan } }).exports;

assertEqNaN(m.test(), f64_snan);
assertEqNaN(m.global, f64_snan);

// NaN propagation behavior.
var constantCache = new Map;
function getConstant(code) {
    if (typeof code === 'number')
        return code;
    if (constantCache.has(code)) {
        return constantCache.get(code);
    }
    let type = code.indexOf('f32') >= 0 ? 'f32' : 'f64';
    let val = wasmEvalText(`(module (func (result ${type}) ${code}) (export "" 0))`)();
    constantCache.set(code, val);
    return val;
}

function test(type, opcode, snan_code, rhs_code, qnan_val) {
    var snan_val = getConstant(snan_code);
    var rhs = getConstant(rhs_code);

    // Test all forms:
    // - (constant, constant),
    // - (constant, variable),
    // - (variable, constant),
    // - (variable, variable)
    assertEqNaN(wasmEvalText(`(module (func (result ${type}) (${type}.${opcode} ${snan_code} ${rhs_code})) (export "" 0))`)(), qnan_val);
    assertEqNaN(wasmEvalText(`(module (func (param ${type}) (result ${type}) (${type}.${opcode} (get_local 0) ${rhs_code})) (export "" 0))`)(snan_val), qnan_val);
    assertEqNaN(wasmEvalText(`(module (func (param ${type}) (result ${type}) (${type}.${opcode} ${snan_code} (get_local 0))) (export "" 0))`)(rhs), qnan_val);
    assertEqNaN(wasmEvalText(`(module (func (param ${type}) (param ${type}) (result ${type}) (${type}.${opcode} (get_local 0) (get_local 1))) (export "" 0))`)(snan_val, rhs), qnan_val);
}

var f32_zero = '(f32.const 0)';
var f64_zero = '(f64.const 0)';

var f32_one = '(f32.const 1)';
var f64_one = '(f64.const 1)';

var f32_negone = '(f32.const -1)';
var f64_negone = '(f64.const -1)';

// x - 0.0 doesn't get folded into x:
test('f32', 'sub', f32_snan_code, f32_zero, f32_qnan);
test('f64', 'sub', f64_snan_code, f64_zero, f64_qnan);

// x * 1.0 doesn't get folded into x:
test('f32', 'mul', f32_snan_code, f32_one, f32_qnan);
test('f32', 'mul', f32_one, f32_snan_code, f32_qnan);

test('f64', 'mul', f64_snan_code, f64_one, f64_qnan);
test('f64', 'mul', f64_one, f64_snan_code, f64_qnan);

// x * -1.0 doesn't get folded into -x:
test('f32', 'mul', f32_snan_code, f32_negone, f32_qnan);
test('f32', 'mul', f32_negone, f32_snan_code, f32_qnan);

test('f64', 'mul', f64_snan_code, f64_negone, f64_qnan);
test('f64', 'mul', f64_negone, f64_snan_code, f64_qnan);

// x / -1.0 doesn't get folded into -1 * x:
test('f32', 'div', f32_snan_code, f32_negone, f32_qnan);
test('f64', 'div', f64_snan_code, f64_negone, f64_qnan);

// min doesn't get folded when one of the operands is a NaN
test('f32', 'min', f32_snan_code, f32_zero, f32_qnan);
test('f32', 'min', f32_zero, f32_snan_code, f32_qnan);

test('f64', 'min', f64_snan_code, f64_zero, f64_qnan);
test('f64', 'min', f64_zero, f64_snan_code, f64_qnan);

// ditto for max
test('f32', 'max', f32_snan_code, f32_zero, f32_qnan);
test('f32', 'max', f32_zero, f32_snan_code, f32_qnan);

test('f64', 'max', f64_snan_code, f64_zero, f64_qnan);
test('f64', 'max', f64_zero, f64_snan_code, f64_qnan);

setJitCompilerOption('wasm.test-mode', 0);
