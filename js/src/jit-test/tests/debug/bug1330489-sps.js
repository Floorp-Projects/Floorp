// |jit-test| test-also=--wasm-compiler=optimizing; error: TestComplete

load(libdir + "asserts.js");

if (!wasmDebuggingEnabled())
    throw "TestComplete";

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration("arm-simulator"))
    throw "TestComplete";

enableGeckoProfiling();
enableSingleStepProfiling();

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function () {};");

let module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "a" "b" (func $imp (result i32)))
        (memory 1 1)
        (table 2 2 anyfunc)
        (elem (i32.const 0) $imp $def)
        (func $def (result i32) (i32.load (i32.const 0)))
        (type $v2i (func (result i32)))
        (func $call (param i32) (result i32) (call_indirect (type $v2i) (local.get 0)))
        (export "call" (func $call))
    )
`));

let instance = new WebAssembly.Instance(module, {
    a: { b: function () { throw "test"; } }
});

try {
    instance.exports.call(0);
    assertEq(false, true);
} catch (e) {
    assertEq(e, "test");
}

disableGeckoProfiling();
throw "TestComplete";
