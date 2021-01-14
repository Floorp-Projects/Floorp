var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (global $g (export "g") (mut f64) (f64.const 0))
  (func $p (param f64) (result f64) (local.get 0))
  (func (export "f") (param f64) (result f64)
    (global.set $g (f64.neg (local.get 0)))
    (call $p (local.get 0))))`)));
assertEq(ins.exports.f(3), 3);
assertEq(ins.exports.g.value, -3);


