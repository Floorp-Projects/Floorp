load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});

let g_discardSource = newGlobal({
  discardSource: true,
});

assertErrorMessage(() => {
  // Script is compiled with discardSource=false, and is executed in global
  // with discardSource=true.
  g.evaluate("function f() { (function(){})(); }; f();", {
    global: g_discardSource,
  });
}, g_discardSource.Error, "discardSource option mismatch");


let g_instrumentation = newGlobal({newCompartment: true});
let dbg_instrumentation = new Debugger();
let gdbg_instrumentation = dbg_instrumentation.addDebuggee(g_instrumentation);

gdbg_instrumentation.setInstrumentation(
  gdbg_instrumentation.makeDebuggeeValue((kind, script, offset) => {
  }),
  ["breakpoint"]
);

assertErrorMessage(() => {
  // Script is compiled with instrumentationKinds=breakpoint, and is executed
  // in global with instrumentationKinds=0.
  g_instrumentation.evaluate("function f() { (function(){})(); }; f();", {
    global: g,
  });
}, g.Error, "instrumentationKinds mismatch");
