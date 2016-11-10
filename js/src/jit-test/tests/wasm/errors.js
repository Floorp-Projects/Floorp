load(libdir + "wasm.js");
load(libdir + "wasm-binary.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const RuntimeError = WebAssembly.RuntimeError;

function isWasmFunction(name) {
    return /^wasm-function\[\d*\]$/.test(name)
}

function parseStack(stack) {
    var frames = stack.split('\n');
    assertEq(frames[frames.length-1], "");
    frames.length--;
    return frames.map(frame => {
        var res = frame.match(/^(.*)@.*:(\d*):(\d*)$/);
        assertEq(res !== null, true);
        return {name: res[1], line: Number(res[2]), column: Number(res[3])};
    });
}

function testExn(opcode, binary, type, msg, exn) {
    assertEq(exn instanceof type, true);
    assertEq(msg.test(exn.message), true);

    var stack = parseStack(exn.stack);
    assertEq(stack.length > 1, true);
    var innermost = stack[0];
    assertEq(isWasmFunction(innermost.name), true);
    assertEq(innermost.line, exn.lineNumber);
    assertEq(innermost.column, exn.columnNumber);

    assertEq(exn.lineNumber > 0, true);
    assertEq(exn.columnNumber, 1);
    assertEq(binary[exn.lineNumber], opcode);

    return {stack, binary};
}

function test(opcode, text, type, msg) {
    var binary = new Uint8Array(wasmTextToBinary(text));
    var exn;
    try {
        new Instance(new Module(binary));
    } catch (e) {
        exn = e;
    }

    return testExn(opcode, binary, type, msg, exn);
}

function testAccess(opcode, text, width, type, msg) {
    var binary = new Uint8Array(wasmTextToBinary(text));
    var instance = new Instance(new Module(binary));
    for (var base of [64 * 1024, 2 * 64 * 1024, Math.pow(2, 30), Math.pow(2, 31), Math.pow(2, 32) - 1]) {
        for (var sub = 0; sub < width; sub++) {
            var ptr = base - sub;
            let exn = null;
            try {
                instance.exports[''](ptr);
            } catch (e) {
                exn = e;
            }
            testExn(opcode, binary, type, msg, exn);
        }
    }
}

function testLoad(opcode, optext, width, type, msg) {
    var text = `(module (memory 1) (func (export "") (param i32) (drop (${optext} (get_local 0)))))`;
    testAccess(opcode, text, width, type, msg);
}

function testStore(opcode, optext, consttext, width, type, msg) {
    var text = `(module (memory 1) (func (export "") (param i32) (${optext} (get_local 0) (${consttext}.const 0))))`;
    testAccess(opcode, text, width, type, msg);
}

test(UnreachableCode, '(module (func unreachable) (start 0))', RuntimeError, /unreachable executed/);
test(I32DivSCode, '(module (func (drop (i32.div_s (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32DivUCode, '(module (func (drop (i32.div_u (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32RemSCode, '(module (func (drop (i32.rem_s (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32RemUCode, '(module (func (drop (i32.rem_u (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32RemUCode, '(module (func (drop (i32.rem_u (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32TruncSF32Code, '(module (func (drop (i32.trunc_s/f32 (f32.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I32TruncSF64Code, '(module (func (drop (i32.trunc_s/f64 (f64.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I32TruncUF32Code, '(module (func (drop (i32.trunc_u/f32 (f32.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I32TruncUF64Code, '(module (func (drop (i32.trunc_u/f64 (f64.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I64TruncSF32Code, '(module (func (drop (i64.trunc_s/f32 (f32.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I64TruncSF64Code, '(module (func (drop (i64.trunc_s/f64 (f64.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I64TruncUF32Code, '(module (func (drop (i64.trunc_u/f32 (f32.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I64TruncUF64Code, '(module (func (drop (i64.trunc_u/f64 (f64.const 1e30)))) (start 0))', RuntimeError, /integer overflow/);
test(I32TruncSF32Code, '(module (func (drop (i32.trunc_s/f32 (f32.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I32TruncSF64Code, '(module (func (drop (i32.trunc_s/f64 (f64.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I32TruncUF32Code, '(module (func (drop (i32.trunc_u/f32 (f32.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I32TruncUF64Code, '(module (func (drop (i32.trunc_u/f64 (f64.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I64TruncSF32Code, '(module (func (drop (i64.trunc_s/f32 (f32.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I64TruncSF64Code, '(module (func (drop (i64.trunc_s/f64 (f64.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I64TruncUF32Code, '(module (func (drop (i64.trunc_u/f32 (f32.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(I64TruncUF64Code, '(module (func (drop (i64.trunc_u/f64 (f64.const nan)))) (start 0))', RuntimeError, /invalid conversion to integer/);
test(CallIndirectCode, '(module (table 1 anyfunc) (func (call_indirect 0 (i32.const 0))) (start 0))', RuntimeError, /indirect call to null/);
test(CallIndirectCode, '(module (table 1 anyfunc) (func (call_indirect 0 (i32.const 1))) (start 0))', RuntimeError, /index out of bounds/);
test(CallIndirectCode, '(module (table anyfunc (elem $blah)) (func (call_indirect 0 (i32.const 0))) (func $blah (param i32)) (start 0))', RuntimeError, /indirect call signature mismatch/);
testLoad(I32Load8S, 'i32.load8_s', 1, RuntimeError, /index out of bounds/);
testLoad(I32Load8U, 'i32.load8_u', 1, RuntimeError, /index out of bounds/);
testLoad(I32Load16S, 'i32.load16_s', 2, RuntimeError, /index out of bounds/);
testLoad(I32Load16U, 'i32.load16_u', 2, RuntimeError, /index out of bounds/);
testLoad(I64Load8S, 'i64.load8_s', 1, RuntimeError, /index out of bounds/);
testLoad(I64Load8U, 'i64.load8_u', 1, RuntimeError, /index out of bounds/);
testLoad(I64Load16S, 'i64.load16_s', 2, RuntimeError, /index out of bounds/);
testLoad(I64Load16U, 'i64.load16_u', 2, RuntimeError, /index out of bounds/);
testLoad(I64Load32S, 'i64.load32_s', 4, RuntimeError, /index out of bounds/);
testLoad(I64Load32U, 'i64.load32_u', 4, RuntimeError, /index out of bounds/);
testLoad(I32Load, 'i32.load', 4, RuntimeError, /index out of bounds/);
testLoad(I64Load, 'i64.load', 8, RuntimeError, /index out of bounds/);
testLoad(F32Load, 'f32.load', 4, RuntimeError, /index out of bounds/);
testLoad(F64Load, 'f64.load', 8, RuntimeError, /index out of bounds/);
testStore(I32Store8, 'i32.store8', 'i32', 1, RuntimeError, /index out of bounds/);
testStore(I32Store16, 'i32.store16', 'i32', 2, RuntimeError, /index out of bounds/);
testStore(I64Store8, 'i64.store8', 'i64', 1, RuntimeError, /index out of bounds/);
testStore(I64Store16, 'i64.store16', 'i64', 2, RuntimeError, /index out of bounds/);
testStore(I64Store32, 'i64.store32', 'i64', 4, RuntimeError, /index out of bounds/);
testStore(I32Store, 'i32.store', 'i32', 4, RuntimeError, /index out of bounds/);
testStore(I64Store, 'i64.store', 'i64', 8, RuntimeError, /index out of bounds/);
testStore(F32Store, 'f32.store', 'f32', 4, RuntimeError, /index out of bounds/);
testStore(F64Store, 'f64.store', 'f64', 8, RuntimeError, /index out of bounds/);

// Stack overflow isn't really a trap or part of the formally-specified
// semantics of call so use the same InternalError as JS and use the bytecode
// offset of the function body (which happens to start with the number of
// local entries).
test(4 /* = num locals */, '(module (func (local i32 i64 f32 f64) (call 0)) (start 0))', InternalError, /too much recursion/);

// Test whole callstack.
var {stack, binary} = test(UnreachableCode, `(module
    (type $v2v (func))
    (func $a unreachable)
    (func $b call $a)
    (func $c call $b)
    (table anyfunc (elem $c))
    (func $d (call_indirect $v2v (i32.const 0)))
    (func $e call $d)
    (start $e)
)`, RuntimeError, /unreachable executed/);
const N = 5;
assertEq(stack.length > N, true);
var lastLine = stack[0].line;
for (var i = 1; i < N; i++) {
    assertEq(isWasmFunction(stack[i].name), true);
    assertEq(stack[i].line > lastLine, true);
    lastLine = stack[i].line;
    assertEq(binary[stack[i].line], i == 3 ? CallIndirectCode : CallCode);
    assertEq(stack[i].column, 1);
}
