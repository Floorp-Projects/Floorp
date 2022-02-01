// |jit-test| skip-if: helperThreadCount() === 0

// Test that we don't crash while logging allocations and there is
// off-main-thread compilation. OMT compilation will allocate functions and
// regexps, but we just punt on measuring that accurately.

const root = newGlobal({newCompartment: true});
root.eval("this.dbg = new Debugger()");
root.dbg.addDebuggee(this);
root.dbg.memory.trackingAllocationSites = true;

offThreadCompileToStencil(
  "function foo() {\n" +
  "  print('hello world');\n" +
  "}"
);
