// SIMD JS API
//
// As of 31 March 2020 the SIMD spec is very light on information about the JS
// API, and what it has is ridden with misspellings, grammatical errors, and
// apparent redundancies.  The rules below represent my best effort at
// understanding the intent of the spec.  As far as I can tell, the rules for
// v128 are intended to match the rules for i64 in the Wasm MVP.

// RULE: v128 cannot cross the JS/wasm boundary as a function parameter.
//
// A wasm function that:
//  - takes or returns v128
//  - was imported into wasm
//  - is ultimately a JS function
// should always throw TypeError when called from wasm.
//
// TODO: we need to add tests here that assert that the JIT exit stubs don't
// bypass the guard.

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (import "m" "v128_param" (func $f (param v128)))
    (import "m" "v128_return" (func $g (result v128)))
    (func (export "v128_param")
      (call $f (v128.const i32x4 0 0 0 0)))
    (func (export "v128_result")
      (drop (call $g))))`)),
                                   {m:{v128_param: (x) => 0,
                                       v128_return: () => 0}});

assertErrorMessage(() => ins.exports.v128_param(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

assertErrorMessage(() => ins.exports.v128_result(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// RULE: v128 cannot cross the JS/wasm boundary as a function parameter.
//
// A wasm function that:
//  - takes or returns v128
//  - is exported from wasm
//  - is ultimately a true wasm function
// should always throw TypeError when called from JS.
//
// TODO: we need to add tests here that assert that the JIT entry stubs don't
// bypass the guard, note there are two kinds of JIT entry stubs.

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (func (export "v128_param") (param v128) (result i32)
      (i32.const 0))
    (func (export "v128_result") (result v128)
      (v128.const i32x4 0 0 0 0)))`)));

assertErrorMessage(() => ins.exports.v128_param(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

assertErrorMessage(() => ins.exports.v128_result(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// RULE: The rules about v128 passing into or out of a function apply even when
// an imported JS function is re-exported and is then called.

var newfn = (x) => x;
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (import "m" "fn" (func $f (param v128) (result v128)))
    (export "newfn" (func $f)))`)),
                                   {m:{fn: newfn}});
assertErrorMessage(() => ins.exports.newfn(3),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// RULE: WebAssembly.Global of type v128 is constructable from JS with a default
// value.

var gi = new WebAssembly.Global({value: "v128"});
var gm = new WebAssembly.Global({value: "v128", mutable:true});

// RULE: WebAssembly.Global constructor for type v128 does not accept any value
// but throws TypeError.

assertErrorMessage(() => new WebAssembly.Global({value: "v128"}, 37),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// RULE: WebAssembly.Global of type v128 have getters and setters that throw
// TypeError when called from JS.

assertErrorMessage(() => gi.value,
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);
assertErrorMessage(() => gi.valueOf(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);
assertErrorMessage(() => gm.value = 0,
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);


