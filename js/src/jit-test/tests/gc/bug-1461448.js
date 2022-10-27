// |jit-test| skip-if: helperThreadCount() === 0

gczeal(0);

let lfPreamble = `
  var lfOffThreadGlobal = newGlobal({newCompartment: true});
  for (lfLocal in this)
    try {} catch(lfVare5) {}
`;
evaluate(lfPreamble);
evaluate(`
  var g = newGlobal({newCompartment: true});
  var dbg = new Debugger;
  var gw = dbg.addDebuggee(g);
  for (lfLocal in this)
    if (!(lfLocal in lfOffThreadGlobal))
      try {
        lfOffThreadGlobal[lfLocal] = this[lfLocal];
      } catch(lfVare5) {}
  var g = newGlobal({newCompartment: true});
  var gw = dbg.addDebuggee(g);
`);
lfOffThreadGlobal.offThreadCompileToStencil(`
  gcparam("markStackLimit", 1);
  grayRoot()[0] = "foo";
`);
var stencil = lfOffThreadGlobal.finishOffThreadStencil();
lfOffThreadGlobal.evalStencil(stencil);
eval(`
  var lfOffThreadGlobal = newGlobal({newCompartment: true});
  try { evaluate(\`
    gczeal(18, 1);
    grayRoot()[0] = "foo";
    let inst = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    \\\`(module
       (memory (export "memory") 1 1)
     )\\\`
    )));
\`); } catch(exc) {}
`);
