// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;
const Memory = WebAssembly.Memory;

// Test for stale heap pointers after resize

// Grow directly from builtin call:
assertEq(evalText(`(module
    (memory 1)
    (func $test (result i32)
        (i32.store (i32.const 0) (i32.const 1))
        (i32.store (i32.const 65532) (i32.const 10))
        (grow_memory (i32.const 99))
        (i32.store (i32.const 6553596) (i32.const 100))
        (i32.add
            (i32.load (i32.const 0))
            (i32.add
                (i32.load (i32.const 65532))
                (i32.load (i32.const 6553596)))))
    (export "test" $test)
)`).exports.test(), 111);

// Grow during call_import:
var exports = evalText(`(module
    (import $imp "a" "imp")
    (memory 1)
    (func $grow (grow_memory (i32.const 99)))
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
)`, {a:{imp() { exports.grow() }}}).exports;

setJitCompilerOption("baseline.warmup.trigger", 2);
setJitCompilerOption("ion.warmup.trigger", 4);
for (var i = 0; i < 10; i++)
    assertEq(exports.test(), 111);

// Grow during call_indirect:
var mem = new Memory({initial:1});
var tbl = new Table({initial:1, element:"anyfunc"});
var exports1 = evalText(`(module
    (import "a" "mem" (memory 1))
    (func $grow
        (i32.store (i32.const 65532) (i32.const 10))
        (grow_memory (i32.const 99))
        (i32.store (i32.const 6553596) (i32.const 100)))
    (export "grow" $grow)
)`, {a:{mem}}).exports;
var exports2 = evalText(`(module
    (import "a" "tbl" (table 1))
    (import "a" "mem" (memory 1))
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
)`, {a:{tbl, mem}}).exports;
tbl.set(0, exports1.grow);
assertEq(exports2.test(), 111);

// Test for coherent length/contents

var mem = new Memory({initial:1});
new Int32Array(mem.buffer)[0] = 42;
var mod = new Module(textToBinary(`(module
    (import "" "mem" (memory 1))
    (func $gm (param i32) (grow_memory (get_local 0)))
    (export "grow_memory" $gm)
    (func $cm (result i32) (current_memory))
    (export "current_memory" $cm)
    (func $ld (param i32) (result i32) (i32.load (get_local 0)))
    (export "load" $ld)
    (func $st (param i32) (param i32) (i32.store (get_local 0) (get_local 1)))
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
