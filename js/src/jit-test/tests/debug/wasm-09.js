// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled()
// Tests debugEnabled state of wasm when allowUnobservedWasm == true.

load(libdir + "asserts.js");

// Checking that there are no offsets are present in a wasm instance script for
// which debug mode was not enabled.
function getWasmScriptWithoutAllowUnobservedWasm(wast) {
    var sandbox = newGlobal({newCompartment: true});
    var dbg = new Debugger();
    dbg.allowUnobservedWasm = true;
    dbg.addDebuggee(sandbox);
    sandbox.eval(`
        var wasm = wasmTextToBinary('${wast}');
        var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
    `);
    // Attaching after wasm instance is created.
    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    return wasmScript;
}

var wasmScript1 = getWasmScriptWithoutAllowUnobservedWasm('(module (func (nop)))');
var wasmLines1 = wasmScript1.source.text.split('\n');
assertEq(wasmScript1.startLine, 1);
assertEq(wasmScript1.lineCount, 0);
assertEq(wasmLines1.every((l, n) => wasmScript1.getLineOffsets(n + 1).length == 0), true);

// Checking that we must not resolve any location for any offset in a wasm
// instance which debug mode was not enabled.
var wasmScript2 = getWasmScriptWithoutAllowUnobservedWasm('(module (func (nop)))');
for (var i = wasmTextToBinary('(module (func (nop)))').length - 1; i >= 0; i--)
    assertThrowsInstanceOf(() => wasmScript2.getOffsetLocation(i), Error);
