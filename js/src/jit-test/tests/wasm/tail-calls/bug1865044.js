// |jit-test| --more-compartments; skip-variant-if: --wasm-test-serialization, true; skip-variant-if: --wasm-compiler=ion, true; skip-if: !wasmGcEnabled() || !('Function' in WebAssembly)

a = newGlobal();
a.b = this;
a.eval("(" + function () {
    Debugger(b).onExceptionUnwind = function () {}
} + ")()");
function c(d) {
  binary = wasmTextToBinary(d)
  e = new WebAssembly.Module(binary)
  return new WebAssembly.Instance(e)
}
f = c(`
(type $g 
  (func (param i64 i64 funcref) (result i64))
)          
(func (export "vis") (param i64 i64 funcref) (result i64) 
  local.get 0 
  local.get 1
  local.get 2
  local.get 2
  ref.cast (ref $g)
  return_call_ref $g
)
`);
h = f.exports["vis"];
i = new WebAssembly.Function({
  parameters: [ "i64", "i64", "funcref" ], results: ["i64"]   
}, function () {});

assertErrorMessage(
  () => h(3n, 1n, i),
  TypeError, /can't convert undefined to BigInt/
);
