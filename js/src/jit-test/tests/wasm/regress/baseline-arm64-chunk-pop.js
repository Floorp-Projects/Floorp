var bin = wasmTextToBinary(
    `(module
       (func (export "f4786") (result i32)
         (local i32 i64 i64 i64 f32)
         i32.const 1
         local.tee 0
         local.get 0
         local.get 0
         local.get 0
         local.get 0
         local.get 0
         local.get 0
         local.get 0
         if (result i32)
           local.get 0
         else
           local.get 0
           local.tee 0
           local.get 0
           br_if 1
         end
         drop
         drop
         drop
         drop
         drop
         drop
         drop))`);
var ins = new WebAssembly.Instance(new WebAssembly.Module(bin));
ins.exports.f4786();

