# JS shell shutdown ordering

evalInWorker(`
  var lfGlobal = newGlobal();
  lfGlobal.offThreadCompileScript(\`{ let x; throw 42; }\`);
`);
