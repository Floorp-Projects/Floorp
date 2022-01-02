// Test the fast paths along which funcref values flow between wasm and JS.
// Currently, these paths always go from wasm into JS; never from JS into wasm.

setJitCompilerOption("baseline.warmup.trigger", 5);
setJitCompilerOption("ion.warmup.trigger", 10);

var iter = 10000;
var objs = [];
var index = 0;

var ins0 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func $f (export "f") (result i32) (i32.const 37))
      (func $g (export "g") (result i32) (i32.const 42)))`)));
objs.push(ins0.exports.f);
objs.push(ins0.exports.g);

// Tests that:
//
//  * wasm->js calls along the fast path pass funcref correctly on entry to JS.
//    This is tested by JS observing that the value that it receives is the same
//    that was passed into wasm in the first place.
//
//  * js->wasm calls along the fast path pass funcref correctly on return from
//    wasm.  This is tested by returning the passed-in value and observing in
//    JS that the value is the same as the value that was passed in.

// `p` will be a register argument on most platforms.

function js_funcref_regarg(p) {
    if (p !== objs[index])
        throw new Error("Bad argument at " + index);
}

// `p` will be a stack argument on all platforms.  In the internal ABI, the
// funcref parameter ends up in a stack slot in the call to js_funcref_stackarg,
// thus we're testing a different path in the stub code.  (This holds for
// architectures with up to 10 register arguments, which covers all tier-1
// platforms as of 2019.)

function js_funcref_stackarg(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, p) {
    if (p !== objs[index])
        throw new Error("Bad argument at " + index);
}

var ins1 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "" "f" (func $f (param funcref)))
       (import "" "g" (func $g (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param i32) (param i32) (param i32) (param i32) (param i32)
                               (param funcref)))
       (func (export "run1") (param funcref) (result funcref)
         (call $f (local.get 0))
         (local.get 0))
       (func (export "run2") (param funcref) (result funcref)
         (call $g (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4)
                  (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9)
                  (local.get 0))
         (local.get 0)))`)),
                                   {"":{f: js_funcref_regarg, g: js_funcref_stackarg}});

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins1.exports.run1(objs[index]);
    if (res !== objs[index])
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}

index = 0;
for ( let i=0; i < iter; i++ ) {
    let res = ins1.exports.run2(objs[index]);
    if (res !== objs[index])
        throw new Error("Bad return at " + index);
    index = (index + 1) % objs.length;
}
