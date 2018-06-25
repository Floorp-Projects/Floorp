var i = 0;
while(i++ < 500) {
  evalInWorker(`
    setJitCompilerOption("baseline.warmup.trigger", 10);
  `);
  let m = parseModule("");
  m.declarationInstantiation();
}

