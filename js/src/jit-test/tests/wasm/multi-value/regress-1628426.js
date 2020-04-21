// |jit-test| skip-if: !wasmDebuggingIsSupported()
var g20 = newGlobal({
    newCompartment: true
});
g20.parent = this;
g20.eval("Debugger(parent).onEnterFrame = function() {};");

let bytes = wasmTextToBinary(`
   (module
     (func $dup (param i32) (result i32 i32)
       (local.get 0)
       (local.get 0)
       (i32.const 2)
       (i32.mul))
     (func $main (export "main") (param i32 i32) (result i32)
       (local.get 1)
       (call $dup)
       (i32.sub)))`);

let instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));

assertEq(instance.exports.main(0, 1), -1)
