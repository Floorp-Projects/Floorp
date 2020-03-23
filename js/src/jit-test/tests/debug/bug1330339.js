// |jit-test| test-also=--wasm-compiler=ion; error: TestComplete

if (!wasmDebuggingIsSupported())
     throw "TestComplete";

let module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func" (func))
        (func (export "test")
         call 0 ;; calls the import, which is func #0
        )
    )
`));

let imports = {
  global: {
    func: function () {
        let g = newGlobal({newCompartment: true});
        let dbg = new Debugger(g);
        dbg.onExceptionUnwind = function (frame) {
            frame.older;
        };
        g.eval("throw new Error();");
    }
  }
};
let instance = new WebAssembly.Instance(module, imports);

try {
    instance.exports.test();
    assertEq(false, true);
} catch (e) {
    assertEq(e.constructor.name, 'Error');
}

throw "TestComplete";
