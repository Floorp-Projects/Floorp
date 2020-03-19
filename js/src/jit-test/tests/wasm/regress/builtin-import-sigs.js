var code = wasmTextToBinary(`(module
    (import $one "" "builtin")
    (import $two "" "builtin" (param i32))
    (import $three "" "builtin" (result i32))
    (import $four "" "builtin" (result f32) (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32))
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
