// |jit-test| skip-if: !wasmDebuggingEnabled()

function c() {
    var frame1 = dbg.getNewestFrame();
    assertEq(frame1.script.format, "js");
    assertEq(frame1.script.displayName, "c");
    assertEq(frame1.offset > 0, true);

    var frame2 = frame1.older;
    assertEq(frame2.script.format, "wasm");
    assertEq(frame2.offset > 0, true);

    var frame3 = frame2.older;
    assertEq(frame3.script.format, "js");
    assertEq(frame3.script.displayName, "test");
    assertEq(frame3.offset > 0, true);
}

var bin = wasmTextToBinary(`(module(import "m" "f" (func $f))(func (export "test")call $f))`);
var dbg = newGlobal({newCompartment: true}).Debugger(this);
var mod = new WebAssembly.Module(bin);
var inst = new WebAssembly.Instance(mod, {m: {f: c}});

function test() {
    for (var i = 0; i < 20; i++) {
        inst.exports.test();
    }
}
test();
