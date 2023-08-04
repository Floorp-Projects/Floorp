setJitCompilerOption("baseline.warmup.trigger", 5);
setJitCompilerOption("ion.warmup.trigger", 10);

var iter = 1000;
let objs = WasmExternrefValues;
var index = 0;

// Tests that:
//
//  * wasm->js calls along the fast path unbox externref correctly on entry to JS.
//    This is tested by JS observing that the value that it receives is the same
//    that was passed into wasm in the first place.
//
//  * js->wasm calls along the fast path box JS values into externref correctly on
//    entry into wasm.  This is tested by the precise same test as the case
//    above.
//
//  * js->wasm calls along the fast path unbox externref correctly on return from
//    wasm.  This is tested by returning the passed-in value and observing in
//    JS that the value is the same as the value that was passed in.

// `p` will be a register argument on most platforms.

function js_externref_regarg(p) {
    if (p !== objs[index] && !Number.isNaN(objs[index]))
        throw new Error("Bad argument at " + index);
}

// `p` will be a stack argument on all platforms.  In the internal ABI, the
// externref parameter ends up in a stack slot in the call to js_externref_stackarg,
// thus we're testing a different path in the stub code.  (This holds for
// architectures with up to 10 register arguments, which covers all tier-1
// platforms as of 2019.)

function js_externref_stackarg(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, p) {
    if (p !== objs[index] && !Number.isNaN(objs[index]))
        throw new Error("Bad argument at " + index);
}

var ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "" "f" (func $f (param externref)))
       (import "" "g" (func $g (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param externref)))
       (func (export "run1") (param externref) (result externref)
         (call $f (local.get 0))
         (local.get 0))
       (func (export "run2") (param externref) (result externref)
         (call $g (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4)
                  (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9)
                  (local.get 0))
         (local.get 0)))`)),
                                   {"":{f: js_externref_regarg, g: js_externref_stackarg}});

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins1.exports.run1(objs[index]);
    if (res !== objs[index] && !Number.isNaN(objs[index]))
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins1.exports.run2(objs[index]);
    if (res !== objs[index] && !Number.isNaN(objs[index]))
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}

// Test that:
//
//  * wasm->js calls along the fast path box JS values into externref correctly on
//    return from JS.  This is tested by returing a value from JS through wasm
//    back into JS and then checking that the object is the same as the one that
//    was returned.

function js_returns_externref(p) {
    return objs[index];
}

var ins2 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "" "f" (func $f (result externref)))
       (import "" "g" (func $g (param externref)))
       (func (export "run1") (result externref)
         (local $tmp externref)
         (local.set $tmp (call $f))
         (call $g (local.get $tmp))
         (local.get $tmp)))`)),
                                    {"":{f: js_returns_externref, g: js_externref_regarg}});

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins2.exports.run1();
    if (res !== objs[index] && !Number.isNaN(objs[index]))
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}
