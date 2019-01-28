// |jit-test| skip-if: !wasmDebugSupport()
//
var mod = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (func (export "func_0") (result i32)
         call 0
        )
    )
`));

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("(" + function() {
    var dbg = Debugger(parent);
    dbg.onEnterFrame = function(frame) {}
} + ")()");

processModule(mod);
processModule(mod);
processModule(mod);
processModule(mod);

mod = new WebAssembly.Module(wasmTextToBinary(`
(module (export "func_0" $func1) (func $func1))
`));

processModule(mod);
processModule(mod);
processModule(mod);
processModule(mod);

function processModule(module) {
    try {
        new WebAssembly.Instance(module).exports.func_0();
    } catch(ex) {}
}
