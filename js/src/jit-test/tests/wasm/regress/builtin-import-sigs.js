var code = wasmTextToBinary(`(module
    (import "" "builtin" (func $one))
    (import "" "builtin" (func $two (param i32)))
    (import "" "builtin" (func $three (result i32)))
    (import "" "builtin" (func $four (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)))
    (func (export "run")
        (call $one)
        (call $two (i32.const 0))
        (drop (call $three))
        (drop (call $four (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f32.const 0)))
    )
)`);
var m = new WebAssembly.Module(code);
var i = new WebAssembly.Instance(m, {'':{builtin:Math.sin}});
i.exports.run();
