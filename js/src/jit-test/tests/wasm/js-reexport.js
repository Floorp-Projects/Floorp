// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;
const Table = WebAssembly.Table;

function accum(...args) {
    var sum = 0;
    for (var i = 0; i < args.length; i++)
        sum += args[i];
    return sum;
}

var e = wasmEvalText(`(module
    (import $a "" "a" (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32))
    (import $b "" "b" (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32))
    (import $c "" "c" (param f64 f64 f64 f64 f64 f64 f64 f64 f64 f64) (result f64))
    (import $d "" "d" (param i32 f32 f64 i32 f32 f64 i32 f32 f64 i32) (result f64))
    (func (export "a") (result i32)
        i32.const 1 i32.const 2 i32.const 3 i32.const 4 i32.const 5
        i32.const 6 i32.const 7 i32.const 8 i32.const 9 i32.const 10
        call $a
    )
    (func (export "b") (result f32)
        f32.const 1.1 f32.const 2.1 f32.const 3.1 f32.const 4.1 f32.const 5.1
        f32.const 6.1 f32.const 7.1 f32.const 8.1 f32.const 9.1 f32.const 10.1
        call $b
    )
    (func (export "c") (result f64)
        f64.const 1.2 f64.const 2.2 f64.const 3.2 f64.const 4.2 f64.const 5.2
        f64.const 6.2 f64.const 7.2 f64.const 8.2 f64.const 9.2 f64.const 10.2
        call $c
    )
    (func (export "d") (result f64)
        i32.const 1 f32.const 2.1 f64.const 3.1 i32.const 4 f32.const 5.1
        f64.const 6.1 i32.const 7 f32.const 8.3 f64.const 9.3 i32.const 10
        call $d
    )
)`, {"":{a:accum, b:accum, c:accum, d:accum, e:accum}}).exports;
assertEq(e.a(), 55);
assertEq(e.b(), 56);
assertEq(e.c(), 57);
assertEq(e.d(), 56);

setJitCompilerOption("baseline.warmup.trigger", 5);
setJitCompilerOption("ion.warmup.trigger", 10);

var e = wasmEvalText(`(module
    (import $a "" "a" (param i32 f64) (result f64))
    (export "a" $a)
)`, {"":{a:(a,b)=>a+b}}).exports;
for (var i = 0; i < 100; i++)
    assertEq(e.a(1.5, 2.5), 3.5);
