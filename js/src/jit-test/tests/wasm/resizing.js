const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;
const Memory = WebAssembly.Memory;
const RuntimeError = WebAssembly.RuntimeError;

// ======
// MEMORY
// ======

// Test for stale heap pointers after resize

// Grow directly from builtin call:
wasmFullPass(`(module
    (memory 1)
    (func $test (result i32)
        (i32.store (i32.const 0) (i32.const 1))
        (i32.store (i32.const 65532) (i32.const 10))
        (drop (memory.grow (i32.const 99)))
        (i32.store (i32.const 6553596) (i32.const 100))
        (i32.add
            (i32.load (i32.const 0))
            (i32.add
                (i32.load (i32.const 65532))
                (i32.load (i32.const 6553596)))))
    (export "run" $test)
)`, 111);

// Grow during import call:
var exports = wasmEvalText(`(module
    (import $imp "" "imp")
    (memory 1)
    (func $grow (drop (memory.grow (i32.const 99))))
    (export "grow" $grow)
    (func $test (result i32)
        (i32.store (i32.const 0) (i32.const 1))
        (i32.store (i32.const 65532) (i32.const 10))
        (call $imp)
        (i32.store (i32.const 6553596) (i32.const 100))
        (i32.add
            (i32.load (i32.const 0))
            (i32.add
                (i32.load (i32.const 65532))
                (i32.load (i32.const 6553596)))))
    (export "test" $test)
)`, {"":{imp() { exports.grow() }}}).exports;

setJitCompilerOption("baseline.warmup.trigger", 2);
setJitCompilerOption("ion.warmup.trigger", 4);
for (var i = 0; i < 10; i++)
    assertEq(exports.test(), 111);

// Grow during call_indirect:
var mem = new Memory({initial:1});
var tbl = new Table({initial:1, element:"funcref"});
var exports1 = wasmEvalText(`(module
    (import "" "mem" (memory 1))
    (func $grow
        (i32.store (i32.const 65532) (i32.const 10))
        (drop (memory.grow (i32.const 99)))
        (i32.store (i32.const 6553596) (i32.const 100)))
    (export "grow" $grow)
)`, {"":{mem}}).exports;
var exports2 = wasmEvalText(`(module
    (import "" "tbl" (table 1 funcref))
    (import "" "mem" (memory 1))
    (type $v2v (func))
    (func $test (result i32)
        (i32.store (i32.const 0) (i32.const 1))
        (call_indirect $v2v (i32.const 0))
        (i32.add
            (i32.load (i32.const 0))
            (i32.add
                (i32.load (i32.const 65532))
                (i32.load (i32.const 6553596)))))
    (export "test" $test)
)`, {"":{tbl, mem}}).exports;
tbl.set(0, exports1.grow);
assertEq(exports2.test(), 111);

// Test for coherent length/contents

var mem = new Memory({initial:1});
new Int32Array(mem.buffer)[0] = 42;
var mod = new Module(wasmTextToBinary(`(module
    (import "" "mem" (memory 1))
    (func $gm (param i32) (result i32) (memory.grow (local.get 0)))
    (export "grow_memory" $gm)
    (func $cm (result i32) (memory.size))
    (export "current_memory" $cm)
    (func $ld (param i32) (result i32) (i32.load (local.get 0)))
    (export "load" $ld)
    (func $st (param i32) (param i32) (i32.store (local.get 0) (local.get 1)))
    (export "store" $st)
)`));
var exp1 = new Instance(mod, {"":{mem}}).exports;
var exp2 = new Instance(mod, {"":{mem}}).exports;
assertEq(exp1.current_memory(), 1);
assertEq(exp1.load(0), 42);
assertEq(exp2.current_memory(), 1);
assertEq(exp2.load(0), 42);
mem.grow(1);
assertEq(mem.buffer.byteLength, 2*64*1024);
new Int32Array(mem.buffer)[64*1024/4] = 13;
assertEq(exp1.current_memory(), 2);
assertEq(exp1.load(0), 42);
assertEq(exp1.load(64*1024), 13);
assertEq(exp2.current_memory(), 2);
assertEq(exp2.load(0), 42);
assertEq(exp2.load(64*1024), 13);
exp1.grow_memory(2);
assertEq(exp1.current_memory(), 4);
exp1.store(3*64*1024, 99);
assertEq(exp2.current_memory(), 4);
assertEq(exp2.load(3*64*1024), 99);
assertEq(mem.buffer.byteLength, 4*64*1024);
assertEq(new Int32Array(mem.buffer)[3*64*1024/4], 99);

// Fail at maximum

var mem = new Memory({initial:1, maximum:2});
assertEq(mem.buffer.byteLength, 1 * 64*1024);
assertEq(mem.grow(1), 1);
assertEq(mem.buffer.byteLength, 2 * 64*1024);
assertErrorMessage(() => mem.grow(1), RangeError, /failed to grow memory/);
assertEq(mem.buffer.byteLength, 2 * 64*1024);

// ======
// TABLE
// ======

// Test for stale table base pointers after resize

// Grow during import call:
var exports = wasmEvalText(`(module
    (type $v2i (func (result i32)))
    (import $grow "" "grow")
    (table (export "tbl") 1 funcref)
    (func $test (result i32)
        (i32.add
            (call_indirect $v2i (i32.const 0))
            (block i32
                (call $grow)
                (call_indirect $v2i (i32.const 1)))))
    (func $one (result i32) (i32.const 1))
    (elem (i32.const 0) $one)
    (func $two (result i32) (i32.const 2))
    (export "test" $test)
    (export "two" $two)
)`, {"":{grow() { exports.tbl.grow(1); exports.tbl.set(1, exports.two) }}}).exports;

setJitCompilerOption("baseline.warmup.trigger", 2);
setJitCompilerOption("ion.warmup.trigger", 4);
for (var i = 0; i < 10; i++)
    assertEq(exports.test(), 3);
assertEq(exports.tbl.length, 11);

// Grow during call_indirect:
var exports1 = wasmEvalText(`(module
    (import $grow "" "grow")
    (func $exp (call $grow))
    (export "exp" $exp)
)`, {"":{grow() { exports2.tbl.grow(1); exports2.tbl.set(2, exports2.eleven) }}}).exports;
var exports2 = wasmEvalText(`(module
    (type $v2v (func))
    (type $v2i (func (result i32)))
    (import $imp "" "imp")
    (elem (i32.const 0) $imp)
    (table 2 funcref)
    (func $test (result i32)
        (i32.add
            (call_indirect $v2i (i32.const 1))
            (block i32
                (call_indirect $v2v (i32.const 0))
                (call_indirect $v2i (i32.const 2)))))
    (func $ten (result i32) (i32.const 10))
    (elem (i32.const 1) $ten)
    (func $eleven (result i32) (i32.const 11))
    (export "tbl" table)
    (export "test" $test)
    (export "eleven" $eleven)
)`, {"":{imp:exports1.exp}}).exports;
assertEq(exports2.test(), 21);

// Test for coherent length/contents

var src = wasmEvalText(`(module
    (func $one (result i32) (i32.const 1))
    (export "one" $one)
    (func $two (result i32) (i32.const 2))
    (export "two" $two)
    (func $three (result i32) (i32.const 3))
    (export "three" $three)
)`).exports;
var tbl = new Table({element:"funcref", initial:1});
tbl.set(0, src.one);

var mod = new Module(wasmTextToBinary(`(module
    (type $v2i (func (result i32)))
    (table (import "" "tbl") 1 funcref)
    (func $ci (param i32) (result i32) (call_indirect $v2i (local.get 0)))
    (export "call_indirect" $ci)
)`));
var exp1 = new Instance(mod, {"":{tbl}}).exports;
var exp2 = new Instance(mod, {"":{tbl}}).exports;
assertEq(exp1.call_indirect(0), 1);
assertErrorMessage(() => exp1.call_indirect(1), RuntimeError, /index out of bounds/);
assertEq(exp2.call_indirect(0), 1);
assertErrorMessage(() => exp2.call_indirect(1), RuntimeError, /index out of bounds/);
assertEq(tbl.grow(1), 1);
assertEq(tbl.length, 2);
assertEq(exp1.call_indirect(0), 1);
assertErrorMessage(() => exp1.call_indirect(1), Error, /indirect call to null/);
tbl.set(1, src.two);
assertEq(exp1.call_indirect(1), 2);
assertErrorMessage(() => exp1.call_indirect(2), RuntimeError, /index out of bounds/);
assertEq(tbl.grow(2), 2);
assertEq(tbl.length, 4);
assertEq(exp2.call_indirect(0), 1);
assertEq(exp2.call_indirect(1), 2);
assertErrorMessage(() => exp2.call_indirect(2), Error, /indirect call to null/);
assertErrorMessage(() => exp2.call_indirect(3), Error, /indirect call to null/);

// Fail at maximum

var tbl = new Table({initial:1, maximum:2, element:"funcref"});
assertEq(tbl.length, 1);
assertEq(tbl.grow(1), 1);
assertEq(tbl.length, 2);
assertErrorMessage(() => tbl.grow(1), RangeError, /failed to grow table/);
assertEq(tbl.length, 2);
