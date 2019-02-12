var bin = wasmTextToBinary(
    `(module
       (func (export "f4786") (result i32)
         (local i32 i64 i64 i64 f32)
         i32.const 1
         tee_local 0
         get_local 0
         get_local 0
         get_local 0
         get_local 0
         get_local 0
         get_local 0
         get_local 0
         if i32
           get_local 0
         else
           get_local 0
           tee_local 0
           get_local 0
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

