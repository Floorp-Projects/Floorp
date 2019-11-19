// |jit-test| skip-if: !wasmReftypesEnabled()

setJitCompilerOption("baseline.warmup.trigger", 5);
setJitCompilerOption("ion.warmup.trigger", 10);

// Test that wasm->js calls along the fast path unbox anyref correctly on entry to JS:
// the JS callee should observe the JS value that was passed, no matter what the
// value.
//
// Test that js->wasm calls along the fast path unbox anyref correctly on return
// from wasm: the JS caller should observe that the JS value was returned, no
// matter what the value.

var iter = 1000;
let objs = [null,
            undefined,
            true,
            false,
            {x:1337},
            ["abracadabra"],
            1337,
            13.37,
            "hi",
            37n,
            new Number(42),
            new Boolean(true),
            Symbol("status"),
            () => 1337];
var index = 0;

function test_f(p) {
    if (p !== objs[index])
        throw new Error("Bad argument at " + index);
}

function test_g(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, p) {
    if (p !== objs[index])
        throw new Error("Bad argument at " + index);
}

// The difference between f and g is that in the internal ABI, the anyref
// parameter ends up in a stack slot in the call to $g, thus we're testing a
// different path in the stub code.  (This holds for architectures with up to 10
// register arguments, which covers all tier-1 platforms as of 2019.)

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import $f "" "f" (func (param anyref)))
       (import $g "" "g" (func (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param anyref)))
       (func (export "f") (param anyref) (result anyref)
         (call $f (local.get 0))
         (local.get 0))
       (func (export "g") (param anyref) (result anyref)
         (call $g (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4)
                  (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9)
                  (local.get 0))
         (local.get 0)))`)),
                                   {"":{f: test_f, g: test_g}});

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins.exports.f(objs[index]);
    if (res !== objs[index])
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins.exports.g(objs[index]);
    if (res !== objs[index])
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}
