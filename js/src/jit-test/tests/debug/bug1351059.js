// Tests that onEnterFrame events are enabled when Debugger callbacks set
// before Instance creation.

load(libdir + "asserts.js");

if (!wasmIsSupported())
  quit();

var g = newGlobal();
g.parent = this;
g.onEnterFrameCalled = false;
g.eval(`
    var dbg = new Debugger(parent);
    dbg.onEnterFrame = frame => {
      onEnterFrameCalled = true;
    };
`);
var i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
    (module (func (export "f")))
`)));
i.exports.f();

assertEq(g.onEnterFrameCalled, true);
