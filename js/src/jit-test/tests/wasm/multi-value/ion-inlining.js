
let instance = wasmEvalText(`
   (module
     (func $f (export "f") (result i32 i32 i32)
       (i32.const 0)
       (i32.const 32)
       (i32.const 10)))`);

for (let i = 0; i < 1e3; i++) {
    assertDeepEq(instance.exports.f(), [0, 32, 10]);
}
