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

f32[1] = wasmEvalText('(module (func (result f32) (f32.const nan:0x123456)) (export "" 0))').exports[""]();
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

wasmEvalText('(module (import "" "float32" (param f32)) (func (call 0 (f32.const nan:0x123456))) (export "" 0))', checkBitPatterns).exports[""]();

f64[0] = NaN;
f64[0] = f64[0]; // Force canonicalization.
f64[1] = wasmEvalText('(module (func (result f64) (f64.const nan:0x123456)) (export "" 0))').exports[""]();
assertSameBitPattern(0, 8, 8);

wasmEvalText('(module (import "" "float64" (param f64)) (func (call 0 (f64.const nan:0x123456))) (export "" 0))', checkBitPatterns).exports[""]();

// SANITY CHECKS

// There are two kinds of NaNs: signaling and quiet. Usually, the first bit of
// the payload is used to indicate whether it is quiet (1 for quiet, 0 for
// signaling). Most operations have to transform a signaling NaN into a quiet
// NaN, which prevents some optimizations in WebAssembly.

// A float32 has 32 bits, 23 bits of them being reserved for the mantissa
// (= NaN payload).
var f32_qnan_code = '(f32.const nan:0x600000)';
var f32_snan_code = '(f32.const nan:0x200000)';

var f32_snan = '0x7fa00000';
var f32_qnan = '0x7fe00000';

// A float64 has 64 bits, 1 for the sign, 11 for the exponent, the rest for the
// mantissa (payload).
var f64_nan_base_high = 0x7ff00000;

var f64_snan_code = '(f64.const nan:0x4000000000000)';
var f64_qnan_code = '(f64.const nan:0xc000000000000)';

var f64_snan = '0x7ff4000000000000';
var f64_qnan = '0x7ffc000000000000';

wasmAssert(`(module
    (func $f32_snan (result f32) ${f32_snan_code})
    (func $f32_qnan (result f32) ${f32_qnan_code})
    (func $f64_snan (result f64) ${f64_snan_code})
    (func $f64_qnan (result f64) ${f64_qnan_code})
)`, [
    { type: 'f32', func: '$f32_snan', expected: f32_snan },
    { type: 'f32', func: '$f32_qnan', expected: f32_qnan },
    { type: 'f64', func: '$f64_snan', expected: f64_snan },
    { type: 'f64', func: '$f64_qnan', expected: f64_qnan },
]);

// Actual tests.

wasmAssert(`(module
    (global (mut f32) (f32.const 0))
    (global (mut f64) (f64.const 0))

    ;; An example where a signaling nan gets transformed into a quiet nan:
    ;; snan + 0.0 = qnan
    (func $add (result f32) (f32.add ${f32_snan_code} (f32.const 0)))

    ;; Shouldn't affect NaNess.
    (func $global.set.get_f32 (result f32)
        ${f32_snan_code}
        global.set 0
        global.get 0
    )

    ;; Shouldn't affect NaNess.
    (func $global.set.get_f64 (result f64)
        ${f64_snan_code}
        global.set 1
        global.get 1
    )
)`, [
    { type: 'f32', func: '$add', expected: f32_qnan },
    { type: 'f32', func: '$global.set.get_f32', expected: f32_snan },
    { type: 'f64', func: '$global.set.get_f64', expected: f64_snan },
]);

// NaN propagation behavior.
function test(type, opcode, lhs_code, rhs_code) {
    let qnan_code = type === 'f32' ? f32_qnan : f64_qnan;

    let t = type;
    let op = opcode;

    // Test all forms:
    // - (constant, constant),
    // - (constant, variable),
    // - (variable, constant),
    // - (variable, variable)
    wasmAssert(`(module
        (func $1 (result ${t}) (${t}.${op} ${lhs_code} ${rhs_code}))
        (func $2 (param ${t}) (result ${t}) (${t}.${op} (local.get 0) ${rhs_code}))
        (func $3 (param ${t}) (result ${t}) (${t}.${op} ${lhs_code} (local.get 0)))
        (func $4 (param ${t}) (param ${t}) (result ${t}) (${t}.${op} (local.get 0) (local.get 1)))
    )`, [
        { type, func: '$1', expected: qnan_code },
        { type, func: '$2', args: [lhs_code], expected: qnan_code },
        { type, func: '$3', args: [rhs_code], expected: qnan_code },
        { type, func: '$4', args: [lhs_code, rhs_code], expected: qnan_code },
    ]);
}

var f32_zero = '(f32.const 0)';
var f64_zero = '(f64.const 0)';

var f32_one = '(f32.const 1)';
var f64_one = '(f64.const 1)';

var f32_negone = '(f32.const -1)';
var f64_negone = '(f64.const -1)';

// x - 0.0 doesn't get folded into x:
test('f32', 'sub', f32_snan_code, f32_zero);
test('f64', 'sub', f64_snan_code, f64_zero);

// x * 1.0 doesn't get folded into x:
test('f32', 'mul', f32_snan_code, f32_one);
test('f32', 'mul', f32_one, f32_snan_code);

test('f64', 'mul', f64_snan_code, f64_one);
test('f64', 'mul', f64_one, f64_snan_code);

// x * -1.0 doesn't get folded into -x:
test('f32', 'mul', f32_snan_code, f32_negone);
test('f32', 'mul', f32_negone, f32_snan_code);

test('f64', 'mul', f64_snan_code, f64_negone);
test('f64', 'mul', f64_negone, f64_snan_code);

// x / -1.0 doesn't get folded into -1 * x:
test('f32', 'div', f32_snan_code, f32_negone);
test('f64', 'div', f64_snan_code, f64_negone);

// min doesn't get folded when one of the operands is a NaN
test('f32', 'min', f32_snan_code, f32_zero);
test('f32', 'min', f32_zero, f32_snan_code);

test('f64', 'min', f64_snan_code, f64_zero);
test('f64', 'min', f64_zero, f64_snan_code);

// ditto for max
test('f32', 'max', f32_snan_code, f32_zero);
test('f32', 'max', f32_zero, f32_snan_code);

test('f64', 'max', f64_snan_code, f64_zero);
test('f64', 'max', f64_zero, f64_snan_code);
