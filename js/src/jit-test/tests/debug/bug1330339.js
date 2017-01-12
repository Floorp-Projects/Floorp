// |jit-test| test-also-wasm-baseline; error: TestComplete

if (!wasmIsSupported())
     throw "TestComplete";

let module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func")
        (func (export "test")
         call 0 ;; calls the import, which is func #0
        )
    )
`));

let imports = {
  global: {
    func: function () {
        let g = newGlobal();
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
