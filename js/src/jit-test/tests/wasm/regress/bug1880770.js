// Check proper handling of OOM during segments creation.

var x = {};
Object.defineProperty(x, "", {
  enumerable: true,
  get: function () {
    new WebAssembly.Instance(
      new WebAssembly.Module(
        wasmTextToBinary(
          '(func $f (result f32) f32.const 0)(table (export "g") 1 funcref) (elem (i32.const 0) $f)'
        )
      )
    ).exports.g
      .get(0)
      .type(WebAssembly, "", WebAssembly.Module, {});
  },
});
oomTest(function () {
  Object.values(x);
});
