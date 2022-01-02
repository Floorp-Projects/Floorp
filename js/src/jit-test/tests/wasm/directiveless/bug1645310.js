// |jit-test| --wasm-compiler=optimizing; skip-if: !wasmIsSupported()

// In this case we're setting things up so that wasm can't be compiled, as the
// only available compiler is Ion, and enabling the Debugger will disable Ion.
//
// The test tests that the lazy creation of the WebAssembly object is not
// dependent on whether WebAssembly can be compiled or not: if wasm is supported
// then the WebAssembly object should always be created.  The fact that wasm
// can't be compiled is a separate matter.
//
// IT'S IMPORTANT NOT TO MENTION THE WEBASSEMBLY OBJECT UNTIL AFTER THE DEBUGGER
// HAS BEEN CREATED, AND NOT TO LOAD lib/wasm.js OR OTHER WASM CODE HERE.

var g7 = newGlobal({newCompartment: true});
g7.parent = this;
g7.eval("var dbg = Debugger(parent)");
assertEq(typeof WebAssembly, "object");

// Test that validation works even if compilers are not available.

WebAssembly.validate(wasmTextToBinary('(module (func))'));

// Test that compilation fails with a sensible error.

var bits = wasmTextToBinary('(module (func))');
var msg = /no WebAssembly compiler available/
var exn;

exn = null;
try { new WebAssembly.Module(bits); } catch (e) { exn = e; }
assertEq(Boolean(exn), true);
assertEq(Boolean(String(exn).match(msg)), true);

exn = null;
try { WebAssembly.compile(bits); } catch (e) { exn = e; }
assertEq(Boolean(exn), true);
assertEq(Boolean(String(exn).match(msg)), true);

exn = null;
try { WebAssembly.instantiate(bits); } catch (e) { exn = e; }
assertEq(Boolean(exn), true);
assertEq(Boolean(String(exn).match(msg)), true);

// We do not use wasmStreamingEnabled() here because that checks whether
// compilers are available, and that is precisely what we want to be checking
// ourselves.  But streaming compilation is available only if there are helper
// threads, so that's an OK proxy.

if (helperThreadCount() > 0) {
    exn = null;
    WebAssembly.compileStreaming(bits).catch(e => { exn = e; });
    drainJobQueue();
    assertEq(Boolean(exn), true);
    assertEq(Boolean(String(exn).match(msg)), true);

    exn = null;
    WebAssembly.instantiateStreaming(bits).catch(e => { exn = e; });
    drainJobQueue();
    assertEq(Boolean(exn), true);
    assertEq(Boolean(String(exn).match(msg)), true);
}
