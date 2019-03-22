load(libdir + "wasm-binary.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const CompileError = WebAssembly.CompileError;
const RuntimeError = WebAssembly.RuntimeError;

function getWasmFunctionIndex(line) {
    return Number(line.match(/^wasm-function\[(\d*)\]$/)[1]);
}

function getWasmBytecode(column) {
    return parseInt(column.match(/^0x([0-9a-f]*)$/)[1], 16);
}

function parseStack(stack) {
    var frames = stack.split('\n');
    assertEq(frames[frames.length-1], "");
    frames.length--;
    return frames.map(frame => {
        var res = frame.match(/^(.*)@(.*):(.*):(.*)$/);
        assertEq(res !== null, true);
        return {name: res[1], url: res[2], line: res[3], column: res[4]};
    });
}

function testExn(opcode, binary, type, msg, exn) {
    assertEq(exn instanceof type, true);
    assertEq(msg.test(exn.message), true);

    var stack = parseStack(exn.stack);
    assertEq(stack.length > 1, true);
    var innermost = stack[0];
    var funcIndex = getWasmFunctionIndex(innermost.line);
    var bytecode = getWasmBytecode(innermost.column);
    assertEq(exn.lineNumber, bytecode);
    assertEq(exn.columnNumber, 1);
    assertEq(binary[bytecode], opcode);

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
    var text = `(module (memory 1) (func (export "") (param i32) (drop (${optext} (local.get 0)))))`;
    testAccess(opcode, text, width, type, msg);
}

function testStore(opcode, optext, consttext, width, type, msg) {
    var text = `(module (memory 1) (func (export "") (param i32) (${optext} (local.get 0) (${consttext}.const 0))))`;
    testAccess(opcode, text, width, type, msg);
}

test(UnreachableCode, '(module (func unreachable) (start 0))', RuntimeError, /unreachable executed/);
test(I32DivSCode, '(module (func (drop (i32.div_s (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32DivSCode, '(module (func (drop (i32.div_s (i32.const -2147483648) (i32.const -1)))) (start 0))', RuntimeError, /integer overflow/);
test(I32DivUCode, '(module (func (drop (i32.div_u (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32RemSCode, '(module (func (drop (i32.rem_s (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I32RemUCode, '(module (func (drop (i32.rem_u (i32.const 1) (i32.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I64DivSCode, '(module (func (drop (i64.div_s (i64.const 1) (i64.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I64DivSCode, '(module (func (drop (i64.div_s (i64.const -9223372036854775808) (i64.const -1)))) (start 0))', RuntimeError, /integer overflow/);
test(I64DivUCode, '(module (func (drop (i64.div_u (i64.const 1) (i64.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I64RemSCode, '(module (func (drop (i64.rem_s (i64.const 1) (i64.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
test(I64RemUCode, '(module (func (drop (i64.rem_u (i64.const 1) (i64.const 0)))) (start 0))', RuntimeError, /integer divide by zero/);
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
test(CallIndirectCode, '(module (table 1 funcref) (func (call_indirect 0 (i32.const 0))) (start 0))', RuntimeError, /indirect call to null/);
test(CallIndirectCode, '(module (table 1 funcref) (func (call_indirect 0 (i32.const 1))) (start 0))', RuntimeError, /index out of bounds/);
test(CallIndirectCode, '(module (table funcref (elem $blah)) (func (call_indirect 0 (i32.const 0))) (func $blah (param i32)) (start 0))', RuntimeError, /indirect call signature mismatch/);
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
    (table funcref (elem $c))
    (func $d (call_indirect $v2v (i32.const 0)))
    (func $e call $d)
    (start $e)
)`, RuntimeError, /unreachable executed/);
const N = 5;
assertEq(stack.length > N, true);
assertEq(getWasmFunctionIndex(stack[0].line), 0);
var lastLine = stack[0].line;
for (var i = 1; i < N; i++) {
    assertEq(getWasmFunctionIndex(stack[i].line), i);
    assertEq(stack[i].line > lastLine, true);
    lastLine = stack[i].line;
    assertEq(binary[getWasmBytecode(stack[i].column)], i == 3 ? CallIndirectCode : CallCode);
}

function testCompileError(opcode, text) {
    var binary = new Uint8Array(wasmTextToBinary(text));
    var exn;
    try {
        new Instance(new Module(binary));
    } catch (e) {
        exn = e;
    }

    assertEq(exn instanceof CompileError, true);
    var offset = Number(exn.message.match(/at offset (\d*)/)[1]);
    assertEq(binary[offset], opcode);
}

testCompileError(CallCode, '(module (func $f (param i32)) (func $g call $f))');
testCompileError(I32AddCode, '(module (func (i32.add (i32.const 1) (f32.const 1))))');
testCompileError(EndCode, '(module (func (block i32)))');
