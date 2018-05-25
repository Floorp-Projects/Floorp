// |jit-test| slow; allow-oom

if (typeof oomTest !== 'function' || !wasmIsSupported()) {
    print('Missing oomTest or wasm support in wasm/regress/oom-eval');
    quit();
}

function foo() {
  var g = newGlobal({sameZoneAs: this});
  g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" 0))')));`);
}
oomTest(foo);
