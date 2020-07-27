// |jit-test| skip-if: !wasmDebuggingIsSupported()
// clearAllBreakpoints should clear breakpoints for WASM scripts.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval(`
    var wasm = wasmTextToBinary(
      '(module (func (nop) (nop)) (export "test" (func 0)))');
    var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
    var offsets = wasmCodeOffsets(wasm);
`);
var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];

let count = 0;
wasmScript.setBreakpoint(g.offsets[0], {
  hit: () => {
    count++;
  },
});

g.m.exports.test();
assertEq(count, 1);

g.m.exports.test();
assertEq(count, 2);

dbg.clearAllBreakpoints();

g.m.exports.test();
assertEq(count, 2);
