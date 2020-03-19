var evalInFrame = (function (global) {
    var dbgGlobal = newGlobal({newCompartment: true});
    var dbg = new dbgGlobal.Debugger();
    return function evalInFrame(code) {
        dbg.addDebuggee(global);
        var frame = dbg.getNewestFrame().older;
        frame = frame.older || frame;
        let completion = frame.eval(code);
        return completion.return;
    };
})(this);

const { exports } = wasmEvalText(`
    (module
        (import "global" "func" (param i32) (result i32))
        (func (export "func_0") (param i32)(result i32)
         local.get 0
         call 0
        )
    )
`, {
    global: {
        func: function jscode(i) {
            return evalInFrame(`a = ${i}`);
        }
    }
});

for (i = 0; i < 20; ++i) {
    assertEq(exports.func_0(i), i);
}
