// |jit-test| skip-if: !wasmDebuggingEnabled(); exitstatus:3

function userError() {};

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

g.eval(`
  var wasm = wasmTextToBinary('(module (func (export "test") (nop)))');
  var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
`);

dbg.onEnterFrame = function(frame) {
    if (frame.type == "wasmcall") {
        throw new userError()
    }
}

result = g.eval("m.exports.test()");
