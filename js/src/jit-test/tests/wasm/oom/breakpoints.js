// |jit-test| skip-if: !('oomTest' in this)

var dbgGlobal = newGlobal({newCompartment: true});
var dbg = new dbgGlobal.Debugger();
dbg.addDebuggee(this);

oomTest(() => {
  wasmEvalText(`
    (import "" "" (func $d))
    (func try call $d end)
  `);
});
