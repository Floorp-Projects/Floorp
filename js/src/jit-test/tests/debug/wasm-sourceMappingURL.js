// |jit-test| test-also-wasm-baseline
// Tests that wasm module sourceMappingURL section is parsed.

if (!wasmIsSupported())
  quit();

load(libdir + "asserts.js");
load(libdir + "wasm-binary.js");

var g = newGlobal();
var dbg = new Debugger(g);

var gotScript;
dbg.allowWasmBinarySource = true;
dbg.onNewScript = (script) => {
  gotScript = script;
}

function toU8(array) {
    for (let b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array);
}

function varU32(u32) {
    assertEq(u32 >= 0, true);
    assertEq(u32 < Math.pow(2,32), true);
    var bytes = [];
    do {
        var byte = u32 & 0x7f;
        u32 >>>= 7;
        if (u32 != 0)
            byte |= 0x80;
        bytes.push(byte);
    } while (u32 != 0);
    return bytes;
}

function string(name) {
    var nameBytes = name.split('').map(c => {
        var code = c.charCodeAt(0);
        assertEq(code < 128, true); // TODO
        return code;
    });
    return varU32(nameBytes.length).concat(nameBytes);
}

function appendSourceMappingURL(wasmBytes, url) {
    if (!url)
        return wasmBytes;
    var payload = [...string('sourceMappingURL'), ...string(url)];
    return Uint8Array.from([...wasmBytes, userDefinedId, payload.length, ...payload]);
}
g.toWasm = (wast, url) => appendSourceMappingURL(wasmTextToBinary(wast), url);

// The sourceMappingURL section is not present
g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(toWasm('(module (func) (export "" 0))')));`);
assertEq(gotScript.format, "wasm");
assertEq(gotScript.source.sourceMapURL, null);

// The sourceMappingURL section is present
g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(toWasm('(module (func) (export "a" 0))', 'http://example.org/test')));`);
assertEq(gotScript.format, "wasm");
assertEq(gotScript.source.sourceMapURL, 'http://example.org/test');

// The sourceMapURL is read-only for wasm
assertThrowsInstanceOf(() => gotScript.source.sourceMapURL = 'foo', Error);

// The sourceMappingURL section is present, but is not available when wasm
// binary source is disabled.
dbg.allowWasmBinarySource = false;
g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(toWasm('(module (func) (export "a" 0))', 'http://example.org/test2')));`);
assertEq(gotScript.format, "wasm");
assertEq(gotScript.source.sourceMapURL, null);
