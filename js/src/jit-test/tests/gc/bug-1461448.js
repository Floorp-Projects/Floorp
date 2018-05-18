if (helperThreadCount() === 0)
    quit();

gczeal(0);

let lfPreamble = `
  var lfOffThreadGlobal = newGlobal();
  for (lfLocal in this)
    try {} catch(lfVare5) {}
`;
evaluate(lfPreamble);
evaluate(`
  var g = newGlobal();
  var dbg = new Debugger;
  var gw = dbg.addDebuggee(g);
  for (lfLocal in this)
    if (!(lfLocal in lfOffThreadGlobal))
      try {
        lfOffThreadGlobal[lfLocal] = this[lfLocal];
      } catch(lfVare5) {}
  var g = newGlobal();
  var gw = dbg.addDebuggee(g);
`);
lfOffThreadGlobal.offThreadCompileScript(`
  gcparam("markStackLimit", 1);
  grayRoot()[0] = "foo";
`);
lfOffThreadGlobal.runOffThreadScript();
eval(`
  var lfOffThreadGlobal = newGlobal();
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
