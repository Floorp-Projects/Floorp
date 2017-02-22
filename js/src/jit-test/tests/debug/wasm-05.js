// Tests that wasm module scripts have text line to bytecode offset information
// when source text is generated.

load(libdir + "asserts.js");

// Disabled in aurora (see also bug 1326452).
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
// There shall be total 6 lines with single offset.
assertEq(singleOffsetLines.length === 6, true);
