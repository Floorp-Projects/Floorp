// |jit-test| allow-oom

if (typeof oomTest !== 'function' || !wasmIsSupported()) {
    print('Missing oomTest or wasm support in wasm/regress/oom-eval');
    quit();
}

function foo() {
  var g = newGlobal();
  g.eval(`o = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "" 0))'));`);
}
oomTest(foo);
