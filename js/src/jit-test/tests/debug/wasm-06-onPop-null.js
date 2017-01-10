// |jit-test| test-also-wasm-baseline; exitstatus: 3
// Checking resumption values for 'null' at frame's onPop.

load(libdir + "asserts.js");

if (!wasmIsSupported())
     quit(3);

var g = newGlobal('');
var dbg = new Debugger();
dbg.addDebuggee(g);
sandbox.eval(`
var wasm = wasmTextToBinary('(module (func (nop)) (export "test" 0))');
var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));`);
dbg.onEnterFrame = function (frame) {
    if (frame.type !== "wasmcall") return;
    frame.onPop = function () {
        return null;
    };
};
g.eval("m.exports.test()");
assertEq(false, true);
