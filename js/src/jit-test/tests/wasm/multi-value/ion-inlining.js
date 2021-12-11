
let instance = wasmEvalText(`
   (module
     (func $f (export "f") (result i32 i32 i32)
       (i32.const 0)
       (i32.const 32)
       (i32.const 10)))`);

const options = getJitCompilerOptions();
const jitThreshold = options['ion.warmup.trigger'] * 2 + 2;

for (let i = 0; i < jitThreshold; i++) {
    assertDeepEq(instance.exports.f(), [0, 32, 10]);
}
