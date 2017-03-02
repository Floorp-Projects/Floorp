// |jit-test| test-also-wasm-baseline
// Tests that wasm module scripts have text line to bytecode offset information
// when source text is generated.

load(libdir + "asserts.js");

if (!wasmIsSupported())
     quit();

// Checking if experimental format generates internal source map to binary file
// by querying debugger scripts getLineOffsets.
// (Notice that the source map will not be produced by wasmBinaryToText)
function getAllOffsets(wast) {
  var sandbox = newGlobal('');
  var dbg = new Debugger();
  dbg.addDebuggee(sandbox);
  sandbox.eval(`
    var wasm = wasmTextToBinary('${wast}');
    var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
  `);
  var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
  var lines = wasmScript.source.text.split('\n');
  return lines.map((l, n) => { return { str: l, offsets: wasmScript.getLineOffsets(n + 1) }; });
}

var result1 = getAllOffsets('(module \
  (func (nop)) \
  (func (drop (f32.sqrt (f32.add (f32.const 1.0) (f32.const 2.0))))) \
)');

var nopLine = result1.filter(i => i.str.indexOf('nop') >= 0);
assertEq(nopLine.length, 1);
// The nopLine shall have single offset.
assertEq(nopLine[0].offsets.length, 1);
assertEq(nopLine[0].offsets[0] > 0, true);

var singleOffsetLines = result1.filter(i => i.offsets.length === 1);
// There shall be total 8 lines with single offset.
assertEq(singleOffsetLines.length, 8);

// Checking if all reported offsets can be resolved back to the corresponding
// line number.
function checkOffsetLineNumberMapping(wast, offsetsExpected) {
    var sandbox = newGlobal('');
    var dbg = new Debugger();
    dbg.addDebuggee(sandbox);
    sandbox.eval(`
var wasm = wasmTextToBinary('${wast}');
var module = new WebAssembly.Module(wasm);
imports = {}
for (let descriptor of WebAssembly.Module.imports(module)) {
    imports[descriptor.module] = {}
    switch(descriptor.kind) {
        case "function": imports[descriptor.module][descriptor.name] = new Function(''); break;
    }
}
var instance = new WebAssembly.Instance(module, imports);
`);
    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    assertEq(wasmScript.startLine, 1);
    assertEq(wasmScript.lineCount >= 0, true);
    var lines = wasmScript.source.text.split('\n');
    var offsetsFound = 0;
    lines.forEach(function (l, n) {
        var offsets = wasmScript.getLineOffsets(n + 1);
        if (offsets.length < 1) return;
        assertEq(offsets.length, 1);
        assertEq(offsets[0] > 0, true);
        offsetsFound++;
        var loc = wasmScript.getOffsetLocation(offsets[0]);
        assertEq(loc instanceof Object, true);
        assertEq(loc.isEntryPoint, true);
        assertEq(loc.lineNumber, n + 1);
        assertEq(loc.columnNumber > 0, true);
    });
    assertEq(offsetsFound, offsetsExpected);
}

checkOffsetLineNumberMapping('(module (func))', 1);
checkOffsetLineNumberMapping('(module (func (nop)))', 2);
checkOffsetLineNumberMapping('(module (import "a" "b"))', 0);
checkOffsetLineNumberMapping('(module \
  (func (nop)) \
  (func (drop (f32.sqrt (f32.add (f32.const 1.0) (f32.const 2.0))))) \
)', 8);
checkOffsetLineNumberMapping('(module \
  (func (local i32) i32.const 0 i32.const 1 set_local 0 get_local 0 call 0 i32.add nop drop) \
)', 9);

// Checking that there are no offsets are present in a wasm instance script for
// which debug mode was not enabled.
function getWasmScriptAfterDebuggerAttached(wast) {
    var sandbox = newGlobal('');
    var dbg = new Debugger();
    sandbox.eval(`
        var wasm = wasmTextToBinary('${wast}');
        var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
    `);
    // Attaching after wasm instance is created.
    dbg.addDebuggee(sandbox);
    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    return wasmScript;
}

var wasmScript1 = getWasmScriptAfterDebuggerAttached('(module (func (nop)))');
var wasmLines1 = wasmScript1.source.text.split('\n');
assertEq(wasmScript1.startLine, 1);
assertEq(wasmScript1.lineCount, 0);
assertEq(wasmLines1.every((l, n) => wasmScript1.getLineOffsets(n + 1).length == 0), true);

// Checking that we must not resolve any location for any offset in a wasm
// instance which debug mode was not enabled.
var wasmScript2 = getWasmScriptAfterDebuggerAttached('(module (func (nop)))');
for (var i = wasmTextToBinary('(module (func (nop)))').length - 1; i >= 0; i--)
    assertThrowsInstanceOf(() => wasmScript2.getOffsetLocation(i), Error);
