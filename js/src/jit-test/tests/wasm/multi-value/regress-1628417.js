let bytes = wasmTextToBinary(`
   (module
     (func $main (export "main") (result i32 i32)
       (i32.const 1)
       (i32.const 2)
       (i32.const 0)
       (br_table 0 0)))`);

let instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));

instance.exports.main();
