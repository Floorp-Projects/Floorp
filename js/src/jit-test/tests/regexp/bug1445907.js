// |jit-test| skip-if: !wasmDebuggingEnabled()

// On ARM64, we failed to save x28 properly when generating code for the regexp
// matcher.
//
// There's wasm and Debugger code here because the combination forces the use of
// x28 and exposes the bug when running on the simulator.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
g.eval(`var m = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func (export "test")))')))`);
var re = /./;
dbg.onEnterFrame = function(frame) { re.exec("x") };
result = g.eval("m.exports.test()");
